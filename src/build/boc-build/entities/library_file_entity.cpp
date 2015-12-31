#include "library_file_entity.hpp"
#include "build/boc-build/engine/entity_serializer.hpp"
#include "build/boc-build/engine/entity_deserializer.hpp"
#include "compiler_properties.hpp"
#include <boost/filesystem.hpp>

gorc::library_file_entity_internal_properties::library_file_entity_internal_properties(
        entity_input_stream &is)
    : library_name(is.read_string())
    , objects(is.read_entity_set<object_file_entity>())
{
    std::copy(objects.begin(), objects.end(), std::inserter(dependencies_value,
                                                            dependencies_value.begin()));
}

gorc::library_file_entity_internal_properties::library_file_entity_internal_properties(
        std::string const &library_name,
        std::unordered_set<object_file_entity*> const &objects)
    : library_name(library_name)
    , objects(objects)
{
    std::copy(objects.begin(), objects.end(), std::inserter(dependencies_value,
                                                            dependencies_value.begin()));
}

gorc::library_file_entity_internal_properties::~library_file_entity_internal_properties()
{
    return;
}

gorc::library_file_entity::library_file_entity(entity_input_stream &is)
    : library_file_entity_internal_properties(is)
    , base_file_entity(is.services.get<compiler_properties>()
                                  .make_library_filename(library_name))
{
    return;
}

gorc::library_file_entity::library_file_entity(std::string const &name,
                                               std::unordered_set<object_file_entity*> const &objs,
                                               service_registry const &services)
    : library_file_entity_internal_properties(name, objs)
    , base_file_entity(services.get<compiler_properties>()
                               .make_library_filename(library_name))
{
    return;
}

std::unordered_set<gorc::entity*> const& gorc::library_file_entity::dependencies()
{
    return dependencies_value;
}

bool gorc::library_file_entity::is_dirty()
{
    return !boost::filesystem::is_regular_file(filename);
}

bool gorc::library_file_entity::update(service_registry const &)
{
    // TODO
    return true;
}

void gorc::library_file_entity::serialize(entity_output_stream &os)
{
    os.write_entity_type_id<library_file_entity>();
    os.write_string(library_name);
    os.write_entity_set(objects);
}
