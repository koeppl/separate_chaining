#pragma once

namespace separate_chaining {
    static constexpr size_t INITIAL_BUCKETS = 16; //! number of buckets a separate hash table holds initially
    using bucketsize_type = uint8_t; //! type for storing the sizes of the buckets
    //static constexpr size_t MAX_BUCKET_BYTESIZE = 128;
    static constexpr size_t MAX_BUCKET_BYTESIZE = std::numeric_limits<bucketsize_type>::max(); //! maximum number of elements a bucket can store
    static_assert(MAX_BUCKET_BYTESIZE <= std::numeric_limits<bucketsize_type>::max(), "enlarge separate_chaining::MAX_BUCKET_BYTESIZE for this key type!");


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
    constexpr static bool can_shrink([[maybe_unused]] const size_t newsize, [[maybe_unused]] const size_t bucket = 0) { return true; } // we do not know the actual size, so lets always try
    constexpr static void assign([[maybe_unused]] const size_t size, [[maybe_unused]] const size_t bucket = 0) {}
    constexpr static void clear() {}
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
        DDCHECK(m_maxbucketsizes != nullptr);
        DDCHECK_LT(bucket, m_buckets);
        m_maxbucketsizes[bucket] = size;
    }
    size_t size([[maybe_unused]]const size_t current_size, const size_t bucket) const {
        DDCHECK_LT(bucket, m_buckets);
        return m_maxbucketsizes[bucket];
    }
    void allocate(const size_t new_size)  {
        DDCHECK(m_maxbucketsizes == nullptr);
        m_maxbucketsizes  = reinterpret_cast<bucketsize_type*>  (malloc(new_size*sizeof(bucketsize_type)));
        std::fill(m_maxbucketsizes, m_maxbucketsizes+new_size, static_cast<bucketsize_type>(0));
        ON_DEBUG(m_buckets = new_size;)
    }
    bool needs_resize(const bucketsize_type newsize, const size_t bucket) const {
        return m_maxbucketsizes[bucket] <= newsize;
    }

    bucketsize_type size_after_increment(const bucketsize_type newsize, const size_t bucket) {
        DDCHECK_LT(bucket, m_buckets);
        DDCHECK_LE(newsize, std::numeric_limits<bucketsize_type>::max());
        DDCHECK_LE(static_cast<size_t>(resize(newsize)),std::numeric_limits<bucketsize_type>::max());
        return m_maxbucketsizes[bucket] = resize(newsize);
    }
    void clear() {
        if(m_maxbucketsizes != nullptr) {
            free(m_maxbucketsizes);
            m_maxbucketsizes = nullptr;
        }
    }

    //! checks whether we can shrink the bucket
    bool can_shrink(const size_t current_size, const size_t bucket) const { return current_size < m_maxbucketsizes[bucket]; } // we do not know the actual size, so lets always try

    ~arbitrary_resize() {
        clear();
    }
    /**
     * the number of elements a buckets contains on resizing to a size of at least `newsize`
     * Since this static function is also called from elsewhere, we use `size_t` instead of `bucketsize_type`
     */
    static size_t resize(const size_t newsize) {
        if(newsize < separate_chaining::MAX_BUCKET_BYTESIZE / 4) return newsize*2;
        return std::min<size_t>(newsize * 1.5f, std::numeric_limits<bucketsize_type>::max());; // see https://github.com/facebook/folly/blob/master/folly/docs/FBVector.md
    }
};


}//namespace separate_chaining
