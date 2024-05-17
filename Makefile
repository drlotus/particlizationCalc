ODIR           = obj

ROOTCFLAGS   := $(shell root-config --cflags)
ROOTLIBS     := $(shell root-config --libs)
ROOTGLIBS    := $(shell root-config --glibs)

CXX           = /opt/homebrew/Cellar/llvm/18.1.5/bin/clang++
# CXXFLAGS      = -g -fPIC -O3 -fopenmp -Xpreprocessor # Add -fopenmp for OpenMP support
CXXFLAGS      = -fPIC -O3 -fopenmp -Xpreprocessor # Add -fopenmp for OpenMP support
LD            = /opt/homebrew/Cellar/llvm/18.1.5/bin/clang++
LDFLAGS       = -O3 -fopenmp -lomp  # Add -fopenmp for OpenMP support
# FFLAGS        = -fPIC $(ROOTCFLAGS) -O3
FFLAGS        = -fPIC -O3

CXXFLAGS     += $(ROOTCFLAGS)
# LIBS          = $(ROOTLIBS) $(SYSLIBS)
# GLIBS         = $(ROOTGLIBS) $(SYSLIBS)

LIBS          = $(SYSLIBS)
GLIBS         = $(SYSLIBS)

# _HYDROO        = DecayChannel.o ParticlePDG2.o DatabasePDG2.o UKUtility.o gen.o \
#                 particle.o main.o interpolation.o utils.o elements.o
_HYDROO        =  main.o utils.o element.o engine.o pdg_particle.o surface.o analytical_sol.o bjorken.o geometry.o 
 
# VPATH = src:../UKW
HYDROO = $(patsubst %,$(ODIR)/%,$(_HYDROO))

TARGET = calc
#------------------------------------------------------------------------------

$(TARGET): $(HYDROO)
	$(LD) $(LDFLAGS) $^ -o $@ $(LIBS)
		@echo "$@ done"

clean:
		@rm -f $(ODIR)/*.o $(TARGET)

$(ODIR)/%.o: src/%.cpp 
		$(CXX) $(CXXFLAGS) -c $< -o $@ 

$(ODIR)/%.o: src/%.hpp 
		$(CXX) $(CXXFLAGS) -c $< -o $@ 

# bench:
# 	g++ -c src/bench.cpp -std=c++20 -stdlib=libc++ -lbenchmark -lpthread -O3 -o obj/bench.o
# 	g++ -o bench -lbenchmark -lpthread -O3 obj/bench.o obj/utils.o obj/geometry.o
# clean_bench: 
# 	@rm -f bench