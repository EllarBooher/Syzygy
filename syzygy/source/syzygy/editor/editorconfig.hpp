#pragma once

namespace syzygy
{
enum class GammaTransferFunction
{
    PureGamma,
    sRGB,
    MAX
};
struct EditorConfiguration
{
    GammaTransferFunction transferFunction{GammaTransferFunction::sRGB};
};
} // namespace syzygy