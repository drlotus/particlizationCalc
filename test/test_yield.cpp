#include "../src/utils.h"
#include "../src/geometry.h"
#include "../src/interfaces.h"
#include "../src/fcell.h"
#include "../src/I_engine.h"
#include "../src/yield_calculator.h"
#include "../src/pdg_particle.h"
#include "my_test.h"
namespace
{
    namespace ug = utils::geometry;
    class YieldTest : public my_test
    {
    protected:
        std::shared_ptr<powerhouse::I_engine<hydro::fcell, powerhouse::pdg_particle>> engine;
        utils::program_options opts;
        YieldTest()
        {
            opts.accept_mode = utils::accept_modes::AcceptAll;
            opts.program_mode = utils::program_modes::Yield;
            opts.yield_mode = utils::yield_modes::GlobalEq;
            opts.in_file = "./input/beta.dat";
            opts.out_file = "./output/yield_sample.dat";
            engine = powerhouse::I_engine<hydro::fcell, powerhouse::pdg_particle>::get();
        }
        void SetUp() override
        {
            powerhouse::calculator_factory<hydro::fcell, powerhouse::pdg_particle>::factory()
                ->register_calculator(opts,
                                      []()
                                      {
                                          return std::make_unique<powerhouse::yield_calculator>();
                                      });
        }
        void TearDown() override
        {
        }
    };

    TEST_F(YieldTest, TestParticles)
    {
        auto pion = std::make_unique<powerhouse::pdg_particle>(powerhouse::particle_names::PION_PLUS);

        EXPECT_DOUBLE_EQ(pion->mass(), 0.13957039);
        EXPECT_EQ(pion->name(), "pi");
        EXPECT_EQ(pion->pdg_id(), 211);
        EXPECT_DOUBLE_EQ(pion->Q(), 1);
        EXPECT_DOUBLE_EQ(pion->B(), 0);
        EXPECT_DOUBLE_EQ(pion->S(), 0);
        ASSERT_TRUE(pion->isparticle());
        EXPECT_DOUBLE_EQ(pion->spin(), 0);
        EXPECT_EQ(pion->statistics(), -1);

        int id = powerhouse::particle_names::PION_MINUS;
        auto apion = std::make_unique<powerhouse::pdg_particle>(id);

        EXPECT_DOUBLE_EQ(apion->mass(), 0.13957039);
        EXPECT_EQ(apion->name(), "anti-pi");
        EXPECT_EQ(apion->pdg_id(), 211);
        EXPECT_DOUBLE_EQ(apion->Q(), -1);
        EXPECT_DOUBLE_EQ(apion->B(), 0);
        ASSERT_FALSE(apion->isparticle());
        EXPECT_DOUBLE_EQ(apion->spin(), 0);
        EXPECT_EQ(apion->statistics(), -1);

        auto lambda = std::make_unique<powerhouse::pdg_particle>(powerhouse::particle_names::LAMBDA);
        EXPECT_DOUBLE_EQ(lambda->mass(), 1.115683);
        EXPECT_EQ(lambda->name(), "Lambda");
        EXPECT_EQ(lambda->pdg_id(), 3122);
        EXPECT_DOUBLE_EQ(lambda->Q(), 0);
        EXPECT_DOUBLE_EQ(lambda->B(), 1);
        EXPECT_DOUBLE_EQ(lambda->S(), -1);
        ASSERT_TRUE(lambda->isparticle());
        EXPECT_DOUBLE_EQ(lambda->spin(), 0.5);
        EXPECT_EQ(lambda->statistics(), 1);

        auto alambda = std::make_unique<powerhouse::pdg_particle>(powerhouse::particle_names::LAMBDA_BAR);
        EXPECT_DOUBLE_EQ(alambda->mass(), 1.115683);
        EXPECT_EQ(alambda->name(), "anti-Lambda");
        EXPECT_EQ(alambda->pdg_id(), 3122);
        EXPECT_DOUBLE_EQ(alambda->Q(), 0);
        EXPECT_DOUBLE_EQ(alambda->B(), -1);
        EXPECT_DOUBLE_EQ(alambda->S(), 1);
        ASSERT_FALSE(alambda->isparticle());
        EXPECT_DOUBLE_EQ(alambda->spin(), 0.5);
        EXPECT_EQ(alambda->statistics(), 1);
    }

    TEST_F(YieldTest, TestIfYieldWorks)
    {
        int lines;
        auto surface = read_cells<hydro::fcell>(opts.in_file, 5, lines);
        opts.particle_id = powerhouse::particle_names::PION_PLUS;
        EXPECT_EQ(surface.total(), surface.data().size());
        EXPECT_NO_THROW(engine->init(opts, surface));
        engine->run();
        const auto& output = engine->yield_output();
        for (auto &&row : output)
        {
            ASSERT_TRUE(row.dNd3p >=0) << "at (" << row.pT << "," << row.phi_p <<
            "," << row.y_p << ") dN/d3p = " << row.dNd3p;
        }
        engine->write();
    }

    // TEST_F(YieldTest, TestIfResultsAreUnique)
    // {
    //     int lines;
    //     auto surface = read_cells<hydro::fcell>(opts.in_file, 100, lines);
    //     opts.particle_id = powerhouse::particle_names::PION_PLUS;
    //     EXPECT_EQ(surface.total(), surface.data().size());
    //     EXPECT_NO_THROW(engine->init(opts, surface));
    //     engine->run();
    //     const auto& output = engine->yield_output();
    //     for (auto &&row : output)
    //     {
    //         auto count = std::count_if(output.begin(), output.end(), [&row](const powerhouse::yield_output<hydro::fcell>& yo)
    //         {
    //             return row.phi_p == yo.phi_p && row.pT == yo.pT && row.y_p == yo.y_p;
    //         });
    //         ASSERT_TRUE(count == 1) << "at (" << row.pT << "," << row.phi_p <<
    //         "," << row.y_p << ") there are " << count << " rows";
            
    //     }
    //     engine->write();
    // }
}