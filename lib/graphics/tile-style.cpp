#include "shared.h"
#include "tile.h"

namespace graphics {

const TileStyle default_style_normal = {
    .fill = color::mono(0.2) + color::cyan(0.1),
    .outline = {.weight = 0, .color = color::mono(0.6)},
};

const TileStyle default_style_active = {
    .fill = color::mono(0.2) + color::cyan(0.3),
    .outline = {.weight = 6, .color = color::mono(0.6)},
};

const TileStyleSheet default_style = {
    .bg = color::mono(0.1),
    .normal = default_style_normal,
    .active = default_style_active,
    .text =
        {
            .weight = 2.0,
            .inset = 0.2,
            .color = color::mono(1),
        },
};

} // namespace graphics
