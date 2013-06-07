#pragma once

#include "content/textloader.h"

namespace Gorc {
namespace Content {
namespace Loaders {

class LevelLoader : public TextLoader {
public:
	static const std::vector<boost::filesystem::path> AssetRootPath;

	virtual std::unique_ptr<Asset> Parse(Text::Tokenizer& t, Manager& manager, Diagnostics::Report& report) override;
};

}
}
}
