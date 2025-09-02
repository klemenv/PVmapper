#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"

#include "proto_ca.hpp"
#include "searcher.hpp"

class TestSearcher : public Searcher {
    private:
        Searcher::PvFoundCb _cb = [](const std::string &, const std::string &, uint16_t, const Protocol::Bytes &) {};
    public:
        TestSearcher()
        : Searcher("0.0.0.0", 5053, {1,5, 10}, std::shared_ptr<ChannelAccess>(), _cb)
        {}

        using Searcher::scheduleNextSearch;

        std::list<SearchedPV>& getSearchedPvs()
        {
            return m_searchedPvs;
        }
};

TEST_CASE("Analyze scheduleNextSearch() function") {
    TestSearcher searcher;
    auto& pvs = searcher.getSearchedPvs();

    searcher.addPV("TEST1");
    searcher.addPV("TEST2");
    searcher.addPV("TEST3");

    REQUIRE(pvs.size() == 3);

    auto it = pvs.begin();
    REQUIRE(it->pvname == "TEST3"); it++;
    REQUIRE(it->pvname == "TEST2"); it++;
    REQUIRE(it->pvname == "TEST1");

    searcher.scheduleNextSearch("TEST3", 10);
    it = pvs.begin();
    REQUIRE(it->pvname == "TEST2"); it++;
    REQUIRE(it->pvname == "TEST1"); it++;
    REQUIRE(it->pvname == "TEST3");

    searcher.scheduleNextSearch("TEST3", 10);
    it = pvs.begin();
    REQUIRE(it->pvname == "TEST2"); it++;
    REQUIRE(it->pvname == "TEST1"); it++;
    REQUIRE(it->pvname == "TEST3");

    searcher.scheduleNextSearch("TEST1", 30);
    it = pvs.begin();
    REQUIRE(it->pvname == "TEST2"); it++;
    REQUIRE(it->pvname == "TEST3"); it++;
    REQUIRE(it->pvname == "TEST1");

    searcher.scheduleNextSearch("TEST2", 40);
    it = pvs.begin();
    REQUIRE(it->pvname == "TEST3"); it++;
    REQUIRE(it->pvname == "TEST1"); it++;
    REQUIRE(it->pvname == "TEST2");

    searcher.scheduleNextSearch("TEST3", 1);
    it = pvs.begin();
    REQUIRE(it->pvname == "TEST3"); it++;
    REQUIRE(it->pvname == "TEST1"); it++;
    REQUIRE(it->pvname == "TEST2");
}