#include <celero/Celero.h>
#include "lcp.hpp"
#include "dcheck.hpp"

#include "separate_chaining_map.hpp"
#include <map>

#include <tudocomp/util/compact_hash.hpp>
#include <tudocomp/util/compact_displacement_hash.hpp>
#include <tudocomp/util/compact_sparse_displacement_hash.hpp>
#include <tudocomp/util/compact_sparse_hash.hpp>

CELERO_MAIN

template<class T>
T random_int(const T& maxvalue) {
   return static_cast<T>(std::rand() * (1.0 / (RAND_MAX + 1.0 )) * maxvalue);
}


class Fixture {
   public:
   using key_type = uint32_t;
   using value_type = uint64_t;
   static constexpr size_t NUM_RANGE = 32; // most_significant_bit(NUM_ELEMENTS)
   static_assert(sizeof(key_type)*8 >= NUM_RANGE, "Num range must fit into key_type");

   using map_type = std::map<key_type, value_type>;
   using unordered_type = std::unordered_map<key_type, value_type, SplitMix>;
   using plain_type = separate_chaining_map<plain_key_bucket<key_type>, value_type, hash_mapping_adapter<key_type, SplitMix>>;
   using avx2_type = separate_chaining_map<avx2_key_bucket<key_type>, value_type, hash_mapping_adapter<uint64_t, SplitMix>>;
   using compact_type = separate_chaining_map<varwidth_key_bucket, value_type, xorshift_hash>;
   using elias_type = tdc::compact_sparse_hashmap::compact_sparse_elias_displacement_hashmap_t<value_type>;
   using cleary_type = tdc::compact_sparse_hashmap::compact_sparse_hashmap_t<value_type>;
   using layered_type = tdc::compact_sparse_hashmap::compact_sparse_displacement_hashmap_t<value_type>;

   map_type* m_map = nullptr;
   unordered_type* m_ordered = nullptr;
   plain_type* m_plain = nullptr;
   avx2_type* m_avx = nullptr;
   compact_type* m_compact = nullptr;
   layered_type* m_layered = nullptr;
   elias_type* m_elias = nullptr;
   cleary_type* m_cleary = nullptr;

   size_t m_current_instance = 0;

   void setUp(size_t i) {
      DCHECK_GT(i, 0);
      if(i == m_current_instance) return;
      tearDown();
      m_current_instance = i;

      m_map = new map_type();
      m_ordered = new unordered_type();
      m_plain = new plain_type();
      m_avx = new avx2_type();
      m_compact = new compact_type(NUM_RANGE);

      m_elias = new elias_type(NUM_RANGE);
      m_layered = new layered_type(NUM_RANGE);
      m_cleary = new cleary_type(NUM_RANGE);
      for(size_t i = 0; i < m_current_instance; ++i) {
	 (*m_map)[random_int(1ULL<<NUM_RANGE)] = i;
      }
      for(auto el : *m_map) {
	 (*m_ordered)[el.first] = el.second;
	 (*m_plain)[el.first] = el.second;
	 (*m_avx)[el.first] = el.second;
	 (*m_compact)[el.first] = el.second;
	 (*m_cleary)[el.first] = el.second;
	 (*m_layered)[el.first] = el.second;
	 (*m_elias)[el.first] = el.second;

	 DCHECK_EQ((*m_ordered)[el.first], el.second);
	 DCHECK_EQ((*m_plain)[el.first], el.second);
	 DCHECK_EQ((*m_avx)[el.first], el.second);
	 DCHECK_EQ((*m_compact)[el.first], el.second);
	 DCHECK_EQ((*m_cleary)[el.first], el.second);
	 DCHECK_EQ((*m_elias)[el.first], el.second);
	 DCHECK_EQ((*m_layered)[el.first], el.second);
      }
      DCHECK_EQ(m_ordered->size(), m_map->size());
      DCHECK_EQ(m_plain->size(), m_ordered->size());
      DCHECK_EQ(m_avx->size(), m_ordered->size());
      DCHECK_EQ(m_compact->size(), m_ordered->size());
      DCHECK_EQ(m_cleary->size(), m_ordered->size());
      DCHECK_EQ(m_elias->size(), m_ordered->size());
      DCHECK_EQ(m_layered->size(), m_ordered->size());
   }
   void tearDown() {
      if(m_map != nullptr) {
	 delete m_map;
	 delete m_ordered;
	 delete m_plain;
	 delete m_avx;
	 delete m_compact;
	 delete m_elias;
	 delete m_layered;
	 delete m_cleary;
      }
      m_map = nullptr;
   }
   ~Fixture() {
      tearDown();
   }
};
Fixture static_fixture;



class TableFixture : public celero::TestFixture {


   const size_t m_instance_length;


   std::vector<celero::TestFixture::ExperimentValue> m_problemspace;

   size_t m_current_instance = 0;


   public:

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
      static_fixture.setUp(experimentValue.Value);
   }
   virtual void tearDown() override {
   }


};


BASELINE_F(query, unordered, TableFixture, 0, 100)
{
   const auto& map = *(static_fixture.m_ordered);
   for(auto el : *static_fixture.m_map) {
      celero::DoNotOptimizeAway(map.find(el.first));
   }
}

BENCHMARK_F(query, plain_32, TableFixture, 0, 100)
{
   const auto& plain = *(static_fixture.m_plain);
   for(auto el : *static_fixture.m_map) {
      celero::DoNotOptimizeAway(plain.find(el.first));
   }
}

BENCHMARK_F(query, avx2_32, TableFixture, 0, 100)
{
   const auto& avx = *(static_fixture.m_avx);
   for(auto el : *static_fixture.m_map) {
      celero::DoNotOptimizeAway(avx.find(el.first));
   }
}

BENCHMARK_F(query, compact, TableFixture, 0, 100)
{
   const auto& compact = *(static_fixture.m_compact);
   for(auto el : *static_fixture.m_map) {
      celero::DoNotOptimizeAway(compact.find(el.first));
   }
}

BENCHMARK_F(query, cleary, TableFixture, 0, 100)
{
   auto& cleary = *(static_fixture.m_cleary);
   for(auto el : *static_fixture.m_map) {
      celero::DoNotOptimizeAway(cleary[el.first]);
   }
}

BENCHMARK_F(query, elias, TableFixture, 0, 100)
{
   auto& elias = *(static_fixture.m_elias);
   for(auto el : *static_fixture.m_map) {
      celero::DoNotOptimizeAway(elias[el.first]);
   }
}

BENCHMARK_F(query, layered, TableFixture, 0, 100)
{
   auto& layered = *(static_fixture.m_layered);
   for(auto el : *static_fixture.m_map) {
      celero::DoNotOptimizeAway(layered[el.first]);
   }
}
