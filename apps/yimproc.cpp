//
// LICENSE:
//
// Copyright (c) 2016 -- 2018 Fabio Pellacini
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#include "../yocto/yocto_image.h"
#include "../yocto/yocto_utils.h"
using namespace std::literals;

template <typename Image>
Image make_image(int width, int height);

template <>
ygl::image4f make_image<ygl::image4f>(int w, int h) {
    return ygl::make_image4f(w, h);
}
template <>
ygl::image4b make_image<ygl::image4b>(int w, int h) {
    return ygl::make_image4b(w, h);
}

// Resize image.
template <typename Image>
Image resize_image(const Image& img, int res_width, int res_height) {
    if (!res_width && !res_height)
        ygl::log_fatal("at least argument should be >0");
    if (!res_width)
        res_width = (int)round(img.width * (res_height / (float)img.height));
    if (!res_height)
        res_height = (int)round(img.height * (res_width / (float)img.width));
    auto res = make_image<Image>(res_width, res_height);
    ygl::resize_image(img, res);
    return res;
}

template <typename Image>
Image make_image_grid(const std::vector<Image>& imgs, int tilex) {
    auto nimgs = (int)imgs.size();
    auto width = imgs[0].width * tilex;
    auto height = imgs[0].height * (nimgs / tilex + ((nimgs % tilex) ? 1 : 0));
    auto ret = ygl::make_image4f(width, height, (bool)imgs[0].hdr);
    auto img_idx = 0;
    for (auto& img : imgs) {
        if (img.width != imgs[0].width || img.height != imgs[0].height) {
            ygl::log_fatal("images of different sizes are not accepted");
        }
        auto ox = (img_idx % tilex) * img.width,
             oy = (img_idx / tilex) * img.height;
        if (ret.hdr) {
            for (auto j = 0; j < img.height; j++) {
                for (auto i = 0; i < img.width; i++) {
                    ret.hdr[{i + ox, j + oy}] = img.hdr.at(i, j);
                }
            }
        } else {
            for (auto j = 0; j < img.height; j++) {
                for (auto i = 0; i < img.width; i++) {
                    ret.ldr[{i + ox, j + oy}] = img.ldr.at(i, j);
                }
            }
        }
    }
    return ret;
}

ygl::image4f filter_bilateral(const ygl::image4f& img, float spatial_sigma,
    float range_sigma, const std::vector<ygl::image4f>& features,
    const std::vector<float>& features_sigma) {
    auto filtered = ygl::make_image4f(img.width, img.height);
    auto width = (int)ceil(2.57f * spatial_sigma);
    auto sw = 1 / (2.0f * spatial_sigma * spatial_sigma);
    auto rw = 1 / (2.0f * range_sigma * range_sigma);
    auto fw = std::vector<float>();
    for (auto feature_sigma : features_sigma)
        fw.push_back(1 / (2.0f * feature_sigma * feature_sigma));
    for (auto j = 0; j < img.height; j++) {
        for (auto i = 0; i < img.width; i++) {
            auto av = ygl::zero4f;
            auto aw = 0.0f;
            for (auto fj = -width; fj <= width; fj++) {
                for (auto fi = -width; fi <= width; fi++) {
                    auto ii = i + fi, jj = j + fj;
                    if (ii < 0 || jj < 0) continue;
                    if (ii >= img.width || jj >= img.height) continue;
                    auto uv = ygl::vec2f{float(i - ii), float(j - jj)};
                    auto rgb = img.at(i, j) - img.at(ii, jj);
                    auto w = (float)std::exp(-dot(uv, uv) * sw) *
                             (float)std::exp(-dot(rgb, rgb) * rw);
                    for (auto fi = 0; fi < features.size(); fi++) {
                        auto feat =
                            features[fi].at(i, j) - features[fi].at(ii, jj);
                        w *= exp(-dot(feat, feat) * fw[fi]);
                    }
                    av += w * img.at(ii, jj);
                    aw += w;
                }
            }
            filtered.at(i, j) = av / aw;
        }
    }
    return filtered;
}

ygl::image4f filter_bilateral(
    const ygl::image4f& img, float spatial_sigma, float range_sigma) {
    auto filtered = ygl::make_image4f(img.width, img.height);
    auto width = (int)ceil(2.57f * spatial_sigma);
    auto sw = 1 / (2.0f * spatial_sigma * spatial_sigma);
    auto rw = 1 / (2.0f * range_sigma * range_sigma);
    for (auto j = 0; j < img.height; j++) {
        for (auto i = 0; i < img.width; i++) {
            auto av = ygl::zero4f;
            auto aw = 0.0f;
            for (auto fj = -width; fj <= width; fj++) {
                for (auto fi = -width; fi <= width; fi++) {
                    auto ii = i + fi, jj = j + fj;
                    if (ii < 0 || jj < 0) continue;
                    if (ii >= img.width || jj >= img.height) continue;
                    auto uv = ygl::vec2f{float(i - ii), float(j - jj)};
                    auto rgb = img.at(i, j) - img.at(ii, jj);
                    auto w = std::exp(-dot(uv, uv) * sw) *
                             std::exp(-dot(rgb, rgb) * rw);
                    av += w * img.at(ii, jj);
                    aw += w;
                }
            }
            filtered.at(i, j) = av / aw;
        }
    }
    return filtered;
}

// Merge alpha from one image onto the other.
template <typename T>
void set_alpha(T& img, const T& alpha) {
    for (auto j = 0; j < img.height; j++) {
        for (auto i = 0; i < img.width; i++) {
            img.at(i, j).w = alpha.at(i, j).w;
        }
    }
}

// Set alpha from color.
void set_color_as_alpha(ygl::image4b& img, const ygl::image4b& alpha) {
    for (auto j = 0; j < img.height; j++) {
        for (auto i = 0; i < img.width; i++) {
            auto& p = alpha.at(i, j);
            img.at(i, j).w = (uint8_t)ygl::clamp(
                ((int)p.x + (int)p.y + (int)p.z) / 3, 0, 255);
        }
    }
}

// Set alpha from color.
void set_color_as_alpha(ygl::image4f& img, const ygl::image4f& alpha) {
    for (auto j = 0; j < img.height; j++) {
        for (auto i = 0; i < img.width; i++) {
            auto& p = alpha.at(i, j);
            img.at(i, j).w = (p.x + p.y + p.z) / 3;
        }
    }
}

void multiply(ygl::image4f& img, const ygl::vec4f& scl) {
    for (auto j = 0; j < img.height; j++) {
        for (auto i = 0; i < img.width; i++) { img.at(i, j) *= scl; }
    }
}

void multiply(ygl::image4b& img, const ygl::vec4f& scl) {
    for (auto j = 0; j < img.height; j++) {
        for (auto i = 0; i < img.width; i++) {
            img.at(i, j) = linear_to_srgb(srgb_to_linear(img.at(i, j)) * scl);
        }
    }
}

std::pair<ygl::image4b, ygl::image4f> load_image(
    const std::string& filename, ygl::tonemap_type tonemapper, float exposure) {
    auto hdr = ygl::image4f();
    auto ldr = ygl::image4b();
    if (ygl::is_hdr_filename(filename)) {
        hdr = ygl::load_image4f(filename);
        ldr = ygl::tonemap_image(hdr, tonemapper, exposure);
    } else {
        ldr = ygl::load_image4b(filename);
        hdr = srgb_to_linear(ldr);
    }
    if (ldr.pixels.empty() || hdr.pixels.empty())
        ygl::log_fatal("cannot load image {}", filename);
    return {ldr, hdr};
}

void save_image(const std::string& filename, const ygl::image4b& ldr,
    const ygl::image4f& hdr) {
    if (ygl::is_hdr_filename(filename)) {
        if (!save_image4f(filename, hdr))
            ygl::log_fatal("cannot save image {}", filename);
    } else {
        if (!save_image4b(filename, ldr))
            ygl::log_fatal("cannot save image {}", filename);
    }
}

int main(int argc, char* argv[]) {
    // command line params
    auto parser = ygl::make_parser(argc, argv, "yimproc", "process images");
    auto tonemapper =
        ygl::parse_opt(parser, "--tonemapper", "t", "Tonemapper type.",
            ygl::tonemap_type_names(), ygl::tonemap_type::gamma);
    auto exposure =
        ygl::parse_opt(parser, "--exposure", "t", "Hdr exposure", 0.0f);
    auto resize_width = ygl::parse_opt(
        parser, "--resize-width", "-w", "width (0 to maintain aspect)", 0);
    auto resize_height = ygl::parse_opt(
        parser, "--resize-height", "-h", "height (0 to maintain aspect)", 0);
    auto multiply_color = ygl::parse_opt(parser, "--multiply-color", "",
        "multiply b y this color", ygl::vec4f{1, 1, 1, 1});
    auto spatial_sigma = ygl::parse_opt(
        parser, "--spatial-sigma", "-s", "blur spatial sigma", 0.0f);
    auto range_sigma = ygl::parse_opt(
        parser, "--range-sigma", "-r", "bilateral blur range sigma", 0.0f);
    auto set_alpha_filename = ygl::parse_opt(
        parser, "--set-alpha", "", "set alpha as this image alpha", ""s);
    auto set_color_as_alpha_filename = ygl::parse_opt(parser,
        "--set-color-as-alpha", "", "set alpha as this image color", ""s);
    auto output = ygl::parse_opt(
        parser, "--output", "-o", "output image filename", ""s, true);
    auto filename =
        ygl::parse_arg(parser, "filename", "input image filename", ""s);
    // check parsing
    if (ygl::should_exit(parser)) {
        printf("%s\n", get_usage(parser).c_str());
        exit(1);
    }

    // load
    auto hdr = ygl::image4f();
    auto ldr = ygl::image4b();
    std::tie(ldr, hdr) = load_image(filename, tonemapper, exposure);

    // set alpha
    if (set_alpha_filename != "") {
        auto alpha_hdr = ygl::image4f();
        auto alpha_ldr = ygl::image4b();
        std::tie(alpha_ldr, alpha_hdr) =
            load_image(set_alpha_filename, tonemapper, exposure);
        if (ldr.width != alpha_ldr.width || ldr.height != alpha_ldr.height)
            ygl::log_fatal("bad image size");
        set_alpha(hdr, alpha_hdr);
        set_alpha(ldr, alpha_ldr);
    }

    // set alpha
    if (set_color_as_alpha_filename != "") {
        auto alpha_hdr = ygl::image4f();
        auto alpha_ldr = ygl::image4b();
        std::tie(alpha_ldr, alpha_hdr) =
            load_image(set_color_as_alpha_filename, tonemapper, exposure);
        if (ldr.width != alpha_ldr.width || ldr.height != alpha_ldr.height)
            ygl::log_fatal("bad image size");
        set_color_as_alpha(hdr, alpha_hdr);
        set_color_as_alpha(ldr, alpha_ldr);
    }

    // multiply
    if (multiply_color != ygl::vec4f{1, 1, 1, 1}) {
        multiply(ldr, multiply_color);
        multiply(hdr, multiply_color);
    }

    // resize
    if (resize_width || resize_height) {
        hdr = resize_image(hdr, resize_width, resize_height);
        ldr = resize_image(ldr, resize_width, resize_height);
    }

    // bilateral
    if (spatial_sigma && range_sigma) {
        hdr = filter_bilateral(hdr, spatial_sigma, range_sigma, {}, {});
        ldr = tonemap_image(hdr, tonemapper, exposure);
    }

    // save
    save_image(output, ldr, hdr);

    // done
    return 0;
}
