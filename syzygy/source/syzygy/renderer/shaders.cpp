#include "shaders.hpp"

#include "syzygy/assets/assets.hpp"
#include "syzygy/core/log.hpp"
#include "syzygy/platform/vulkanmacros.hpp"
#include <array>
#include <cassert>
#include <format>
#include <include/spirv/unified1/spirv.h>
#include <spdlog/fmt/bundled/core.h>
#include <spirv_reflect.h>
#include <utility>

namespace syzygy
{
auto generateReflectionData(std::span<uint8_t const> const spirv_bytecode)
    -> ShaderReflectionData
{
    SpvReflectShaderModule module;
    {
        SpvReflectResult const reflectionModuleCreationResult =
            spvReflectCreateShaderModule(
                spirv_bytecode.size_bytes(), spirv_bytecode.data(), &module
            );
        if (reflectionModuleCreationResult != SPV_REFLECT_RESULT_SUCCESS)
        {
            SZG_WARNING(std::format(
                "spvReflectCreateShaderModule failed. SpvReflectResult: {}",
                static_cast<int32_t>(reflectionModuleCreationResult)
            ));
            // TODO: error propagation
            return {};
        }
    }

    uint32_t pushConstantCounts;
    spvReflectEnumeratePushConstantBlocks(
        &module, &pushConstantCounts, nullptr
    );
    std::vector<SpvReflectBlockVariable*> pushConstants(pushConstantCounts);
    spvReflectEnumeratePushConstantBlocks(
        &module, &pushConstantCounts, pushConstants.data()
    );

    std::map<std::string, ShaderReflectionData::PushConstant>
        pushConstantsByEntryPoint{};

    auto const enumeratedEntryPoints{std::span<SpvReflectEntryPoint* const>(
        &module.entry_points, module.entry_point_count
    )};

    for (auto* const pEntryPoint : enumeratedEntryPoints)
    {
        SpvReflectEntryPoint const& entryPoint{*pEntryPoint};

        SpvReflectResult result;
        SpvReflectBlockVariable const* const pPushConstant{
            spvReflectGetEntryPointPushConstantBlock(
                &module, entryPoint.name, &result
            )
        };

        if (result == SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND)
        {
            // No push constant on the entry point
            continue;
        }

        // The only way the result is not success is if
        // 1) the module is null
        // or 2) the entry point does not exist
        // Which we both know to be false.
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        SpvReflectBlockVariable const& pushConstant{*pPushConstant};

        std::vector<ShaderReflectionData::Member> processedMembers{};

        assert(pushConstant.type_description->op == SpvOp::SpvOpTypeStruct);

        // Process members
        std::span<SpvReflectBlockVariable> const members{
            pushConstant.members, pushConstant.member_count
        };
        for (SpvReflectBlockVariable const& member : members)
        {
            // Parse spirv-reflect type descriptions into type-safe form

            uint32_t const offsetBytes{member.offset};

            SpvReflectTypeDescription const& typeDescription{
                *member.type_description
            };
            SpvReflectNumericTraits const& numericTraits{
                typeDescription.traits.numeric
            };

            // SPIR-V type names are empty for built in types ?
            std::string const typeName{
                typeDescription.type_name == nullptr ? ""
                                                     : typeDescription.type_name
            };

            // For early exit, if the type ends up being unsupported

            ShaderReflectionData::Member const unsupportedMember{
                .offsetBytes = offsetBytes,
                .name = member.name,
                .type =
                    ShaderReflectionData::SizedType{
                        .typeData = ShaderReflectionData::UnsupportedType{},

                        .name = typeName,
                        .sizeBytes = member.size,
                        .paddedSizeBytes = member.padded_size,
                    },
            };

            SpvReflectTypeFlags const numericTypesMask{0x0000FFFF};

            if ((typeDescription.type_flags
                 & SpvReflectTypeFlagBits::SPV_REFLECT_TYPE_FLAG_REF)
                != 0U)
            {
                // SpirV-reflect should only add this flag
                // if the type is an OpTypePointer
                processedMembers.push_back(ShaderReflectionData::Member{
                    .offsetBytes = offsetBytes,
                    .name = member.name,
                    .type =
                        ShaderReflectionData::SizedType{
                            .typeData = ShaderReflectionData::Pointer{},
                            .name = typeName,
                            .sizeBytes = member.size,
                            .paddedSizeBytes = member.padded_size,
                        }
                });
            }
            else if ((typeDescription.type_flags & (~numericTypesMask)) == 0U)
            {
                uint32_t const typeFlagComponentTypeMask{0x000000FF};
                ShaderReflectionData::NumericType::ComponentType componentType;
                switch (typeDescription.type_flags & typeFlagComponentTypeMask)
                {
                case SpvReflectTypeFlagBits::SPV_REFLECT_TYPE_FLAG_INT:
                    componentType = ShaderReflectionData::Integer{
                        .signedness =
                            static_cast<bool>(numericTraits.scalar.signedness),
                    };
                    break;
                case SpvReflectTypeFlagBits::SPV_REFLECT_TYPE_FLAG_FLOAT:
                    componentType = ShaderReflectionData::Float{};
                    break;
                default:
                    SZG_WARNING(fmt::format(
                        "Unsupported push constant member type \"{}\" "
                        "for \"{}\"",
                        std::to_string(
                            typeDescription.type_flags
                            & typeFlagComponentTypeMask
                        ),
                        member.name
                    ));
                    processedMembers.push_back(unsupportedMember);
                    continue;
                }

                uint32_t const typeFlagFormatMask{0x0000FF00};
                ShaderReflectionData::NumericType::Format format;
                switch (typeDescription.type_flags & typeFlagFormatMask)
                {
                case 0:
                    format = ShaderReflectionData::Scalar{};
                    break;
                case SpvReflectTypeFlagBits::SPV_REFLECT_TYPE_FLAG_VECTOR:
                    format = ShaderReflectionData::Vector{
                        .componentCount = numericTraits.vector.component_count,
                    };
                    break;
                case SpvReflectTypeFlagBits::SPV_REFLECT_TYPE_FLAG_MATRIX
                    | SpvReflectTypeFlagBits::SPV_REFLECT_TYPE_FLAG_VECTOR:
                    format = ShaderReflectionData::Matrix{
                        .columnCount = numericTraits.matrix.column_count,
                        .rowCount = numericTraits.matrix.row_count,
                    };
                    break;
                default:
                    SZG_WARNING(fmt::format(
                        "Unsupported push constant member format \"{}\" "
                        "for \"{}\"",
                        std::to_string(
                            typeDescription.type_flags & typeFlagFormatMask
                        ),
                        member.name
                    ));
                    processedMembers.push_back(unsupportedMember);
                    continue;
                }

                processedMembers.push_back(ShaderReflectionData::Member{
                    .offsetBytes = offsetBytes,
                    .name = member.name,
                    .type =
                        ShaderReflectionData::SizedType{
                            .typeData =
                                ShaderReflectionData::NumericType{
                                    .componentBitWidth =
                                        numericTraits.scalar.width,
                                    .componentType = componentType,
                                    .format = format,
                                },
                            .name = typeName,
                            .sizeBytes = member.size,
                            .paddedSizeBytes = member.padded_size,
                        }
                });
            }
            else
            {
                SZG_WARNING(fmt::format(
                    "Unsupported push constant member flag types \"{}\" "
                    "for \"{}\"",
                    std::to_string(typeDescription.type_flags),
                    member.name
                ));
                processedMembers.push_back(unsupportedMember);
                continue;
            }
        }

        pushConstantsByEntryPoint.insert(
            {std::string{entryPoint.name},
             ShaderReflectionData::PushConstant{
                 .type{
                     .name = pushConstant.type_description->type_name,
                     .sizeBytes = pushConstant.size,
                     .paddedSizeBytes = pushConstant.padded_size,
                     .members = processedMembers,
                 },
                 .name = pushConstant.name,
                 .layoutOffsetBytes = pushConstant.offset,
             }}
        );
    }

    std::string const defaultEntryPoint{module.entry_point_name};

    spvReflectDestroyShaderModule(&module);

    return ShaderReflectionData{
        .pushConstantsByEntryPoint = pushConstantsByEntryPoint,
        .defaultEntryPoint = defaultEntryPoint,
    };
}

auto ShaderReflectionData::Structure::logicallyCompatible(Structure const& other
) const -> bool
{
    uint32_t memberIndex{0};
    uint32_t otherMemberIndex{0};

    size_t const iterations{0};
    size_t constexpr MAX_ITERATIONS{100U};
    while (iterations < MAX_ITERATIONS)
    {
        if (memberIndex >= members.size()
            || otherMemberIndex >= other.members.size())
        {
            // Reached the end without finding incompatible members,
            // so the rest does not matter.
            return true;
        }

        struct ByteRange
        {
            uint32_t startByte{0};
            uint32_t endUnpaddedByte{0};
            uint32_t endPaddedByte{0};
        };
        auto const getByteRange{[&](Member const& member)
        {
            return ByteRange{
                .startByte = member.offsetBytes,
                .endUnpaddedByte = member.offsetBytes + member.type.sizeBytes,
                .endPaddedByte =
                    member.offsetBytes + member.type.paddedSizeBytes,
            };
        }};

        Member const& member{members[memberIndex]};
        Member const& otherMember{other.members[otherMemberIndex]};

        ByteRange const memberRange{getByteRange(member)};
        ByteRange const otherMemberRange{getByteRange(otherMember)};

        // If the members overlap, their types must be compatible

        // Assert monotonicity so interval calculations are valid
        assert(
            memberRange.endPaddedByte >= memberRange.endUnpaddedByte
            && memberRange.endUnpaddedByte >= memberRange.startByte
        );
        assert(
            otherMemberRange.endPaddedByte >= otherMemberRange.endUnpaddedByte
            && otherMemberRange.endUnpaddedByte >= otherMemberRange.startByte
        );

        if (memberRange.startByte < otherMemberRange.endPaddedByte
            && memberRange.endPaddedByte > otherMemberRange.startByte)
        {
            // For now, require members to be identical.
            if (member.type.typeData != otherMember.type.typeData)
            {
                return false;
            }
        }

        if (memberRange.endUnpaddedByte <= otherMemberRange.endPaddedByte)
        {
            memberIndex += 1;
        }
        else
        {
            otherMemberIndex += 1;
        }
    }
    SZG_ERROR(fmt::format(
        "Ran out of iterations while checking shader structure "
        "compatibility between {} and {}.",
        name,
        other.name
    ));
    return false;
}

auto ShaderReflectionData::NumericType::operator==(NumericType const& other
) const -> bool
{
    return other.componentBitWidth == componentBitWidth
        && other.componentType == componentType && other.format == format;
}

auto ShaderReflectionData::Integer::operator==(Integer const& other) const
    -> bool
{
    return other.signedness == signedness;
}

auto ShaderReflectionData::Vector::operator==(Vector const& other) const -> bool
{
    return other.componentCount == componentCount;
}

auto ShaderReflectionData::Matrix::operator==(Matrix const& other) const -> bool
{
    return other.columnCount == columnCount && other.rowCount == rowCount;
}

auto ShaderObjectReflected::fromBytecode(
    VkDevice const device,
    std::string const& name,
    std::span<uint8_t const> const spirvBytecode,
    VkShaderStageFlagBits const stage,
    VkShaderStageFlags const nextStage,
    std::span<VkDescriptorSetLayout const> const layouts,
    std::span<VkPushConstantRange const> const pushConstantRanges,
    VkSpecializationInfo const specializationInfo
) -> std::optional<ShaderObjectReflected>
{
    ShaderReflectionData const reflectionData{
        generateReflectionData(spirvBytecode)
    };

    ShaderResult<VkShaderEXT> const compilationResult{compileShaderObject(
        device,
        spirvBytecode,
        stage,
        nextStage,
        layouts,
        pushConstantRanges,
        specializationInfo
    )};

    SZG_LOG_VK(compilationResult.result, "Created Shader Object {}", name);
    if (compilationResult.result != VK_SUCCESS)
    {
        return {};
    }

    SZG_INFO(
        fmt::format("Successfully compiled ShaderObjectReflected: {}", name)
    );
    return ShaderObjectReflected(
        name, reflectionData, compilationResult.shader
    );
}

auto ShaderObjectReflected::fromBytecodeReflected(
    VkDevice const device,
    std::string const& name,
    std::span<uint8_t const> const spirvBytecode,
    VkShaderStageFlagBits const stage,
    VkShaderStageFlags const nextStage,
    std::span<VkDescriptorSetLayout const> const layouts,
    VkSpecializationInfo const specializationInfo
) -> std::optional<ShaderObjectReflected>
{
    ShaderReflectionData const reflectionData{
        generateReflectionData(spirvBytecode)
    };

    std::vector<VkPushConstantRange> pushConstantRanges{};

    if (reflectionData.defaultEntryPointHasPushConstant())
    {
        ShaderReflectionData::PushConstant const& pushConstant{
            reflectionData.defaultPushConstant()
        };
        pushConstantRanges.push_back(pushConstant.totalRange(stage));
    }

    ShaderResult<VkShaderEXT> const compilationResult{compileShaderObject(
        device,
        spirvBytecode,
        stage,
        nextStage,
        layouts,
        pushConstantRanges,
        specializationInfo
    )};

    SZG_LOG_VK(compilationResult.result, "Created Shader Object {}", name);
    if (compilationResult.result != VK_SUCCESS)
    {
        return {};
    }

    SZG_INFO(
        fmt::format("Successfully compiled ShaderObjectReflected: {}", name)
    );
    return ShaderObjectReflected(
        name, reflectionData, compilationResult.shader
    );
}

auto ShaderModuleReflected::FromBytecode(
    VkDevice const device,
    std::string const& name,
    std::span<uint8_t const> const spirvBytecode
) -> std::optional<ShaderModuleReflected>
{
    ShaderResult<VkShaderModule> const compilationResult{
        compileShaderModule(device, spirvBytecode)
    };

    if (compilationResult.result != VK_SUCCESS)
    {
        SZG_LOG_VK(
            compilationResult.result, "Failed to create shader module {}", name
        );
        return {};
    }

    ShaderReflectionData const reflectionData{
        generateReflectionData(spirvBytecode)
    };

    SZG_INFO("Successfully compiled ShaderModuleReflected: {}", name);
    return ShaderModuleReflected(
        name, reflectionData, compilationResult.shader
    );
}

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
void ShaderReflectedBase::cleanup(VkDevice const device)
{
    std::visit(
        overloaded{
            [&](VkShaderModule module)
    { vkDestroyShaderModule(device, module, nullptr); },
            [&](VkShaderEXT object)
    { vkDestroyShaderEXT(device, object, nullptr); }
        },
        m_shaderHandle
    );
}

auto compileShaderObject(
    VkDevice const device,
    std::span<uint8_t const> const spirvBytecode,
    VkShaderStageFlagBits const stage,
    VkShaderStageFlags const nextStage,
    std::span<VkDescriptorSetLayout const> const layouts,
    std::span<VkPushConstantRange const> const pushConstantRanges,
    VkSpecializationInfo const specializationInfo
) -> ShaderResult<VkShaderEXT>
{
    VkShaderCreateInfoEXT const createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
        .pNext = nullptr,

        .flags = 0,

        .stage = stage,
        .nextStage = nextStage,

        .codeType = VkShaderCodeTypeEXT::VK_SHADER_CODE_TYPE_SPIRV_EXT,
        .codeSize = spirvBytecode.size(),
        .pCode = spirvBytecode.data(),

        .pName = "main",

        .setLayoutCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data(),

        .pushConstantRangeCount =
            static_cast<uint32_t>(pushConstantRanges.size()),
        .pPushConstantRanges = pushConstantRanges.data(),

        .pSpecializationInfo = &specializationInfo,
    };

    VkShaderEXT shaderObject{VK_NULL_HANDLE};
    VkResult const result{
        vkCreateShadersEXT(device, 1, &createInfo, nullptr, &shaderObject)
    };

    return ShaderResult<VkShaderEXT>{
        .shader = shaderObject,
        .result = result,
    };
}

auto loadShaderObject(
    VkDevice const device,
    std::filesystem::path const& path,
    VkShaderStageFlagBits const stage,
    VkShaderStageFlags const nextStage,
    std::span<VkDescriptorSetLayout const> const layouts,
    VkSpecializationInfo const specializationInfo
) -> std::optional<ShaderObjectReflected>
{
    std::optional<AssetFile> const fileResult{loadAssetFile(path)};
    if (!fileResult.has_value())
    {
        SZG_ERROR("Failed to load file for texture at '{}'", path.string());
        return std::nullopt;
    }
    auto const& file{fileResult.value()};

    return std::optional<ShaderObjectReflected>{
        ShaderObjectReflected::fromBytecodeReflected(
            device,
            file.path.filename().string(),
            file.fileBytes,
            stage,
            nextStage,
            layouts,
            specializationInfo
        )
    };
}

auto loadShaderObject(
    VkDevice const device,
    std::filesystem::path const& path,
    VkShaderStageFlagBits const stage,
    VkShaderStageFlags const nextStage,
    std::span<VkDescriptorSetLayout const> const layouts,
    VkPushConstantRange const rangeOverride,
    VkSpecializationInfo const specializationInfo
) -> std::optional<ShaderObjectReflected>
{
    std::optional<AssetFile> const fileResult{loadAssetFile(path)};
    if (!fileResult.has_value())
    {
        SZG_ERROR("Failed to load file for texture at '{}'", path.string());
        return std::nullopt;
    }
    auto const& file{fileResult.value()};

    std::array<VkPushConstantRange, 1> rangeOverrides{rangeOverride};
    return std::optional<ShaderObjectReflected>{
        ShaderObjectReflected::fromBytecode(
            device,
            file.path.filename().string(),
            file.fileBytes,
            stage,
            nextStage,
            layouts,
            rangeOverrides,
            specializationInfo
        )
    };
}

auto compileShaderModule(
    VkDevice const device, std::span<uint8_t const> const spirvBytecode
) -> ShaderResult<VkShaderModule>
{
    VkShaderModuleCreateInfo const createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,

        .codeSize = spirvBytecode.size(),
        .pCode = reinterpret_cast<uint32_t const*>(spirvBytecode.data()),
    };

    VkShaderModule shaderModule{VK_NULL_HANDLE};
    VkResult const result{
        vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule)
    };

    return ShaderResult<VkShaderModule>{
        .shader = shaderModule,
        .result = result,
    };
}

auto loadShaderModule(VkDevice const device, std::string const& path)
    -> std::optional<ShaderModuleReflected>
{
    std::optional<AssetFile> const fileResult{loadAssetFile(path)};
    if (!fileResult.has_value())
    {
        SZG_ERROR("Failed to load file for texture at '{}'", path);
        return std::nullopt;
    }
    auto const& file{fileResult.value()};

    return std::optional<ShaderModuleReflected>{
        ShaderModuleReflected::FromBytecode(
            device, file.path.filename().string(), file.fileBytes
        )
    };
}
} // namespace syzygy