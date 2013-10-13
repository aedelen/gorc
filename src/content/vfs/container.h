#pragma once

#include "virtual_file.h"
#include <boost/filesystem/path.hpp>

namespace Gorc {
namespace Content {
namespace VFS {

class Container {
public:
	virtual ~Container();

	virtual size_t FileCount() const = 0;
	virtual const VirtualFile& GetVirtualFile(size_t index) const = 0;
	virtual bool IsEpisode() const = 0;
	virtual const VirtualFile& GetEpisode() const = 0;
};

}
}
}
