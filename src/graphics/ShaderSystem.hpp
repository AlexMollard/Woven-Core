#pragma once

#include "pch.hpp"

#include <volk.h>

struct ShaderCompileDesc
{
	std::string filePath;
	std::string entryPoint;
	VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;
};

class ShaderSystem
{
public:
	ShaderSystem();
	~ShaderSystem();

	bool Initialize(VkDevice device, VkDescriptorSetLayout bindlessLayout, const VkPushConstantRange& pushConstants);
	void Shutdown();

	bool CreateShaderObject(const ShaderCompileDesc& desc, VkShaderEXT& outShader);
	void DestroyShader(VkShaderEXT shader);

private:
	bool CompileToSpirv(const ShaderCompileDesc& desc, std::vector<uint32_t>& outSpirv);
	std::string GetModuleName(const std::string& filePath) const;
	std::string GetDiagnosticsString(void* diagnosticsBlob) const;

private:
	VkDevice m_Device = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_BindlessLayout = VK_NULL_HANDLE;
	VkPushConstantRange m_PushConstantRange{};

	struct SlangHandles;
	std::unique_ptr<SlangHandles> m_Slang;

	std::vector<std::string> m_SearchPaths;
};
