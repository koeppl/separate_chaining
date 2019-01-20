#include <celero/Celero.h>
#include "lcp.hpp"
#include "dcheck.hpp"

#include "separate_chaining_map.hpp"

CELERO_MAIN

template<class T>
T random_int(const T& maxvalue) {
   return static_cast<T>(std::rand() * (1.0 / (RAND_MAX + 1.0 )) * maxvalue);
}


class TableFixture : public celero::TestFixture {

   using key_type = uint32_t;
   using value_type = uint64_t;

   const size_t m_instance_length;
   static constexpr size_t NUM_RANGE = 32; // most_significant_bit(NUM_ELEMENTS)
   static_assert(sizeof(key_type)*8 >= NUM_RANGE, "Num range must fit into key_type");


   std::vector<celero::TestFixture::ExperimentValue> m_problemspace;

   size_t m_current_instance = 0;


   public:
   using unordered_type = std::unordered_map<key_type, value_type, SplitMix>;
   using plain_type = separate_chaining_map<plain_key_bucket<key_type>, value_type, hash_mapping_adapter<key_type, SplitMix>>;
   using compact_type = separate_chaining_map<varwidth_key_bucket, value_type, xorshift_hash>;

   unordered_type* m_ordered;
   plain_type* m_plain;
   compact_type* m_compact;

   // const TableInstance& instance() const {
   //    return *m_instances[m_current_instance];
   // }

   TableFixture()
      : m_instance_length(12)
      , m_problemspace(m_instance_length,0)
      // , m_instances( new TableInstance*[m_instance_length])
   {
      for(size_t i = 0; i < m_instance_length; ++i) {
	 m_problemspace[i] = {static_cast<int64_t>(6 + (2ULL<<(i+4)))};
      }
   }
   ~TableFixture() {
      // for(size_t i = 0; i < m_instance_length; ++i) {
	// delete m_instances[i];
      // }
      // delete [] m_instances;
   }


   virtual std::vector<celero::TestFixture::ExperimentValue> getExperimentValues() const override
   {
      return m_problemspace;
   }

   virtual void setUp(const celero::TestFixture::ExperimentValue& experimentValue) override {
      m_current_instance = experimentValue.Value;
      DCHECK_GE(m_instance_length, 0);
      DCHECK_LT(m_current_instance, static_cast<uint64_t>(m_instance_length));
      m_ordered = new unordered_type();
      m_plain = new plain_type();
      m_compact = new compact_type();
      for(size_t i = 0; i < m_current_instance; ++i) {
	 (*m_ordered)[random_int(1ULL<<NUM_RANGE)] = i;
      }
      for(auto el : *m_ordered) {
	 (*m_plain)[el.first] = el.second;
	 (*m_compact)[el.first] = el.second;
      }

   }
   virtual void tearDown() override {
      delete m_ordered;
      delete m_plain;
      delete m_compact;
   }


};


BASELINE_F(query, unordered, TableFixture, 0, 100)
{
   const auto& map = *(m_ordered);
   for(auto el : map) {
      celero::DoNotOptimizeAway(map.find(el.first));
   }
}

BENCHMARK_F(query, plain_32, TableFixture, 0, 100)
{
   const auto& map = *(m_ordered);
   const auto& plain = *(m_plain);
   for(auto el : map) {
      celero::DoNotOptimizeAway(plain.find(el.first));
   }
}

BENCHMARK_F(query, compact, TableFixture, 0, 100)
{
   const auto& map = *(m_ordered);
   const auto& compact = *(m_compact);
   for(auto el : map) {
      celero::DoNotOptimizeAway(compact.find(el.first));
   }
}
