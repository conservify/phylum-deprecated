namespace phylum {

template<typename T, size_t N>
static T *tail_info(uint8_t(&buffer)[N]) {
    auto tail_offset = sizeof(buffer) - sizeof(T);
    return reinterpret_cast<T*>(buffer + tail_offset);
}

inline uint64_t file_block_overhead(const Geometry &geometry) {
    auto sectors_per_block = geometry.sectors_per_block();
    return SectorSize + sizeof(FileBlockTail) + ((sectors_per_block - 2) * sizeof(FileSectorTail));
}

inline uint64_t effective_file_block_size(const Geometry &geometry) {
    return geometry.block_size() - file_block_overhead(geometry);
}

inline uint64_t index_block_overhead(const Geometry &geometry) {
    return SectorSize + sizeof(IndexBlockTail);
}

inline uint64_t effective_index_block_size(const Geometry &geometry) {
    return geometry.block_size() - index_block_overhead(geometry);
}

}
