#pragma once

#include "utils.h"

class TemplateIndex
{
public:
	int folderIndex, templateIndex;
	TemplateIndex(int folderIndex, int templateIndex) {
		this->folderIndex = folderIndex;
		this->templateIndex = templateIndex;
	}
	bool operator==(const TemplateIndex &cmp) const
	{
		return (this->folderIndex == cmp.folderIndex &&
			this->templateIndex == cmp.templateIndex);
	}
};

namespace std {
	template <>
	struct hash<TemplateIndex>
	{
		std::size_t operator()(const TemplateIndex& k) const
		{
			return improveIntHash((k.templateIndex << 4) | (k.folderIndex << 0));
		}
	};
}