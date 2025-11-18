#include <vector>
class UnionFind
{
public:
    uint32_t find(uint32_t current) const;

    uint32_t find_mut(uint32_t current);

    void merge(uint32_t x, uint32_t y);
    uint32_t add();
    uint32_t size() const;

private:
    std::vector<uint32_t> parents;
};
