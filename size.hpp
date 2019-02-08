#pragma once

namespace separate_chaining {
    //static constexpr size_t MAX_BUCKET_BYTESIZE = 128;
    static constexpr size_t MAX_BUCKET_BYTESIZE = 254; //! maximum number of elements a bucket can store
    static constexpr size_t INITIAL_BUCKETS = 16; //! number of buckets a separate hash table holds initially
    using bucketsize_type = uint8_t; //! type for storing the sizes of the buckets
    static_assert(MAX_BUCKET_BYTESIZE < std::numeric_limits<bucketsize_type>::max(), "enlarge separate_chaining::MAX_BUCKET_BYTESIZE for this key type!");


//! let a full bucket grow incrementally on insertion such that there is no need to store the capacity of a bucket (since it is always full)
struct incremental_resize {
    static constexpr size_t INITIAL_BUCKET_SIZE = 1; //! number of elements a bucket can store initially

    constexpr static void allocate([[maybe_unused]]const size_t new_size)  {
    }
    constexpr static size_t size(const size_t current_size, [[maybe_unused]]const size_t bucket = 0) {
        return current_size;
    }

    constexpr static size_t size_after_increment(const size_t newsize, [[maybe_unused]] const size_t bucket = 0) {
        return newsize;
    }
    constexpr static bool needs_resize([[maybe_unused]] const size_t newsize, [[maybe_unused]] const size_t bucket = 0) { return true; }
    constexpr static void assign([[maybe_unused]] const size_t size, [[maybe_unused]] const size_t bucket = 0) {}
};


/**
 * Maintains additionally to `arbitrary_resize` the capacities of all buckets.
**/
class arbitrary_resize {
    public:
    using bucketsize_type = separate_chaining::bucketsize_type; //! used for storing the sizes of the buckets
    static constexpr size_t INITIAL_BUCKET_SIZE = 1; //! number of elements a bucket can store initially

    //private:
    bucketsize_type* m_maxbucketsizes = nullptr; //! size of each bucket
    ON_DEBUG(size_t m_buckets = 0;)

    public:
    arbitrary_resize() = default;

    arbitrary_resize(arbitrary_resize&& o) 
        : m_maxbucketsizes(std::move(o.m_maxbucketsizes))
    {
        ON_DEBUG(m_buckets = std::move(o.m_buckets);)
        o.m_maxbucketsizes = nullptr;
    }
    arbitrary_resize& operator=(arbitrary_resize&& o) {
        m_maxbucketsizes = std::move(o.m_maxbucketsizes);
        ON_DEBUG(m_buckets = std::move(o.m_buckets);)
        o.m_maxbucketsizes = nullptr;
        return *this;
    }
    void assign(const size_t size, const size_t bucket) {
        DCHECK(m_maxbucketsizes != nullptr);
        DCHECK_LT(bucket, m_buckets);
        m_maxbucketsizes[bucket] = size;
    }
    size_t size([[maybe_unused]]const size_t current_size, const size_t bucket) const {
        DCHECK_LT(bucket, m_buckets);
        return m_maxbucketsizes[bucket];
    }
    void allocate(const size_t new_size)  {
        m_maxbucketsizes  = reinterpret_cast<bucketsize_type*>  (malloc(new_size*sizeof(bucketsize_type)));
        ON_DEBUG(m_buckets = new_size;)
    }
    bool needs_resize(const bucketsize_type newsize, const size_t bucket) const {
        return m_maxbucketsizes[bucket] <= newsize;
    }

    bucketsize_type size_after_increment(const bucketsize_type newsize, const size_t bucket) {
        DCHECK_LT(bucket, m_buckets);
        DCHECK_LE(static_cast<size_t>(resize(newsize)),std::numeric_limits<bucketsize_type>::max());
        return m_maxbucketsizes[bucket] = resize(newsize);
    }
    ~arbitrary_resize() {
        if(m_maxbucketsizes != nullptr) {
            free(m_maxbucketsizes);
        }
    }
    /**
     * the number of elements a buckets contains on resizing to a size of at least `newsize`
     * Since this static function is also called from elsewhere, we use `size_t` instead of `bucketsize_type`
     */
    static size_t resize(const size_t newsize) {
        if(newsize < separate_chaining::MAX_BUCKET_BYTESIZE / 4) return newsize*2;
        return newsize * 1.5f; // see https://github.com/facebook/folly/blob/master/folly/docs/FBVector.md
    }
};


}//namespace separate_chaining
