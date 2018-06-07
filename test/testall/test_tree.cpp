#include <gtest/gtest.h>

#include <confs/tree.h>
#include <confs/private.h>
#include <confs/inodes.h>

#include "utilities.h"

using namespace confs;

using StandardTree = BPlusTree<uint64_t, int64_t, 6, 6>;

using std::pair;
using std::map;
using std::vector;

class TreeSuite : public ::testing::Test {
protected:

};

class TreeSuite3Deep : public TreeSuite {
protected:
    static constexpr uint32_t MAX_KEYS = 3;
    static constexpr uint32_t DEPTH_3 = MAX_KEYS + 1 + (MAX_KEYS / 2 + 1) * MAX_KEYS;
    typedef BPlusTree<int32_t, int32_t, MAX_KEYS, MAX_KEYS> TreeType;

    void create_tree(uint32_t depth = DEPTH_3) {
        for (size_t i = 0; i < depth; ++i) {
            tree_.add(i * 2, i * 2);
        }
    }

    TreeType tree_;
    int32_t value_;
};

TEST_F(TreeSuite, SimpleAddLookup) {
    StandardTree tree;

    ASSERT_EQ(tree.lookup(10), 0);

    ASSERT_TRUE(tree.add(10, 128));

    ASSERT_EQ(tree.lookup(10), 128);
    ASSERT_EQ(tree.lookup(20), 0);
}

TEST_F(TreeSuite, AddDuplicate) {
    StandardTree tree;

    ASSERT_EQ(tree.lookup(10), 0);

    ASSERT_TRUE(tree.add(10, 128));
    ASSERT_TRUE(tree.add(10, 128));
}

TEST_F(TreeSuite, SingleLevelMultipleLookup) {
    StandardTree tree;

    tree.add(100, 5738);
    tree.add(10, 1);
    tree.add(22, 2);
    tree.add(8, 3);
    tree.add(3, 4);
    tree.add(17, 5);
    tree.add(9, 6);
    tree.add(30, 7);
    tree.add(20, 8);

    // tree.dump(sdebug, false);

    ASSERT_EQ(tree.lookup(17), 5);
}

TEST_F(TreeSuite, MultipleLevelCreateAndLookup) {
    StandardTree tree;

    auto data = random_data();

    for (auto pair : data) {
        ASSERT_TRUE(tree.add(pair.first, (int64_t)pair.second));
    }

    for (auto pair : data) {
        ASSERT_EQ(tree.lookup(pair.first), (int64_t)pair.second);
    }
}

TEST_F(TreeSuite, SimpleAddAndRemove) {
    StandardTree tree;

    ASSERT_TRUE(tree.add(10, 128));

    ASSERT_TRUE(tree.remove(10));
}

TEST_F(TreeSuite, MultipleLevelRemove) {
    StandardTree tree;

    auto data = random_data();

    for (auto pair : data) {
        ASSERT_TRUE(tree.add(pair.first, pair.second));
    }

    for (auto pair : data) {
        ASSERT_TRUE(tree.remove(pair.first));
    }

    // TODO: Delete is incomplete.
    ASSERT_FALSE(tree.empty());
}

TEST_F(TreeSuite, MultipleLookup) {
    std::vector<uint32_t> inodes;

    StandardTree tree;

    for (auto i = 0; i < 8; ++i) {
        auto inode = (uint32_t)(random() % 2048 + 1024);
        inodes.push_back(inode);

        auto offset = 512;

        for (auto j = 0; j < 128; ++j) {
            tree.add(INodeKey(inode, offset), inode);
            offset += random() % 4096;
        }
    }

    auto first_key = INodeKey(inodes[3], 0);
    auto last_key = INodeKey(inodes[3], ((uint32_t)-1));

    tree.find_all(first_key, last_key);
}

TEST_F(TreeSuite, MultipleLookupRandom) {
    auto map = std::map<int32_t, int32_t>{};
    BPlusTree<int32_t, int32_t, 6, 6> tree;

    srand(1);

    auto value = 1;
    for (auto i = 0; i < 1024; ++i) {
        auto key = random() % UINT32_MAX;
        tree.add(key, value);
        map[key] = value;

        value++;
    }

    for (auto pair : map) {
        ASSERT_EQ(tree.lookup(pair.first), pair.second);
    }
}

TEST_F(TreeSuite3Deep, SimpleInsertAndFind) {
    EXPECT_FALSE(tree_.find(52));

    tree_.add(52, 77);

    EXPECT_TRUE(tree_.find(52, &value_));
    EXPECT_EQ(77, value_);

    EXPECT_FALSE(tree_.find(54));
    EXPECT_FALSE(tree_.find(50));

    // add again = overwrite
    tree_.add(52, 78);
    EXPECT_TRUE(tree_.find(52, &value_));
    EXPECT_EQ(78, value_);

    tree_.add(54, 79);
    tree_.add(50, 80);

    EXPECT_TRUE(tree_.find(52, &value_));
    EXPECT_EQ(78, value_);
    EXPECT_TRUE(tree_.find(54, &value_));
    EXPECT_EQ(79, value_);
    EXPECT_TRUE(tree_.find(50, &value_));
    EXPECT_EQ(80, value_);

    EXPECT_FALSE(tree_.find(55));
    EXPECT_FALSE(tree_.find(51));
    EXPECT_FALSE(tree_.find(0));
}

TEST_F(TreeSuite3Deep, FindLastLessThan) {
    // Empty tree: nothing to find
    EXPECT_FALSE(tree_.find_last_less_then(52));

    // Everything is >= key, nothing to find
    tree_.add(52, 1);
    EXPECT_FALSE(tree_.find_last_less_then(52));

    // This works
    int32_t found_key = 0;
    EXPECT_TRUE(tree_.find_last_less_then(100, &value_, &found_key));
    EXPECT_EQ(1, value_);
    EXPECT_EQ(52, found_key);

    tree_.add(50, 2);
    EXPECT_TRUE(tree_.find_last_less_then(53, &value_, &found_key));
    EXPECT_EQ(1, value_);
    EXPECT_EQ(52, found_key);
    EXPECT_TRUE(tree_.find_last_less_then(52, &value_, &found_key));
    EXPECT_EQ(2, value_);
    EXPECT_EQ(50, found_key);
    EXPECT_TRUE(tree_.find_last_less_then(51, &value_, &found_key));
    EXPECT_EQ(2, value_);
    EXPECT_EQ(50, found_key);
    value_ = -1;
    found_key = -1;
    EXPECT_FALSE(tree_.find_last_less_then(50, &value_, &found_key));
    EXPECT_EQ(-1, value_);
    EXPECT_EQ(-1, found_key);

    tree_.add(49, 3);
    EXPECT_TRUE(tree_.find_last_less_then(52, &value_, &found_key));
    EXPECT_EQ(2, value_);
    EXPECT_EQ(50, found_key);
}

// NOTE: This test currently fails: find_last_less_then does not work when things
// are deleted. JACOB - Is this still true? Steems ok to me.
TEST_F(TreeSuite3Deep, FindLastLessThanDeleted) {
    tree_.add(52, 1);
    tree_.add(50, 2);
    tree_.remove(52);
    EXPECT_TRUE(tree_.find_last_less_then(53, &value_));
    EXPECT_EQ(2, value_);
}

TEST_F(TreeSuite3Deep, FindLastLessThan2) {
    tree_.add(42, -1);
    tree_.add(1804289383, 1);
    tree_.add(1804289383, 2);
    tree_.add( 719885386, 3);
    tree_.add(1804289383, 4);
    tree_.add( 783368690, 5);
    tree_.add( 719885386, 6);

    EXPECT_TRUE(tree_.find_last_less_then(2044897763, &value_));
    EXPECT_EQ(4, value_);

    tree_.add( 304089172, 7);
    EXPECT_TRUE(tree_.find_last_less_then(1804289383, &value_));
    EXPECT_EQ(5, value_);
}

// Runs find, lower_bound, and upper_bound in both the STL set and the B-tree_.
template<typename K, typename V>
static bool do_find(const map<K, V>& std_map, const BPlusTree<K, V, 3, 3>& tree, const K key) {
    auto success = true;

    // Test the weird "find first less than". Not compatible with deletes
    auto i = std_map.lower_bound(key);
    V map_less_than = -2;
    if (i != std_map.begin()) {
        // Go back one
        --i;
        // ASSERT(i->first < key);
        success = success && i->first < key;
        map_less_than = i->second;
    }

    V tree_less_than = -2;
    tree.find_last_less_then(key, &tree_less_than);
    // ASSERT(map_less_than == tree_less_than);
    success = success && map_less_than == tree_less_than;

    i = std_map.find(key);
    V value = 0;
    auto found = tree.find(key, &value);
    if (i != std_map.end()) {
        // ASSERT(found && value == i->second);
        success = success && found && value == i->second;
        return true;
    } else {
        // ASSERT(!found);
        success = false;
    }
    return success;
}

template <typename T>
size_t choose_index(const vector<T>& v) {
    return static_cast<size_t>(random()) % v.size();
}

template <typename T>
T choose(const vector<T>& v) {
    return v[choose_index(v)];
}

TEST_F(TreeSuite3Deep, RandomInsertUnique) {
    srandom(static_cast<unsigned int>(time(NULL)));

    map<int32_t, int32_t> std_map;
    vector<int32_t> values;

    tree_.add(42, -1);
    std_map.insert(std::make_pair(42, -1));
    values.push_back(42);

    for (auto loops = 0; loops < 1000; ++loops) {
        int value;
        if (loops % 2 == 0) {
            value = static_cast<int32_t>(random());
        } else {
            value = choose(values);
        }

        auto insertion = std_map.insert(std::make_pair(value, loops + 1));
        tree_.add(value, loops + 1);
        if (insertion.second) {
            values.push_back(value);
        } else {
            insertion.first->second = loops + 1;
        }

        // Find a value in the set
        value = choose(values);
        ASSERT_TRUE(do_find(std_map, tree_, value));

        // Find a value that is likely not in the set
        value = static_cast<int32_t>(random());
        auto iter = std::find(values.begin(), values.end(), value);
        ASSERT_EQ(iter != values.end(), do_find(std_map, tree_, value));

        // Randomly delete a value from the set
        if (loops % 3 == 2) {
            auto index = choose_index(values);
            value = values[index];
            values.erase(values.begin() + static_cast<ssize_t>(index));
            std_map.erase(std_map.find(value));
            tree_.remove(value);
        }
    }

    for (size_t i = 0; i < values.size(); ++i) {
        ASSERT_TRUE(tree_.find(values[i], &value_));
        auto iter = std_map.find(values[i]);
        ASSERT_EQ(iter->second, value_);
    }
}
