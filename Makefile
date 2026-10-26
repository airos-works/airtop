CXX ?= g++
BUILD ?= build
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -Wno-multichar -MMD -MP
SOURCES := src/main.cpp src/Dashboard.cpp src/Sampler.cpp src/HardwareSensors.cpp src/AuxPanel.cpp
OBJECTS := $(SOURCES:src/%.cpp=$(BUILD)/%.o)
LDLIBS := -lbe -lbnetapi -lnetwork -ldevice

all: $(BUILD)/AirTop
$(BUILD):
	mkdir -p $@
$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
RC ?= rc
XRES ?= xres
MIMESET ?= mimeset
$(BUILD)/AirTop: $(OBJECTS) resources/AirTop.rdef resources/AirTop.hvif
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@.new
	$(RC) -o $(BUILD)/AirTop.rsrc resources/AirTop.rdef
	$(XRES) -o $@.new $(BUILD)/AirTop.rsrc
	$(MIMESET) -f $@.new
	mv $@.new $@
install: all
	bash tools/install.sh $(BUILD)
check:
	mkdir -p $(BUILD)
	$(CXX) -std=c++17 -Wall -Wextra tests/metrics_test.cpp -o $(BUILD)/metrics_test
	$(BUILD)/metrics_test
	$(CXX) -std=c++17 -Wall -Wextra tests/animation_test.cpp -o $(BUILD)/animation_test
	$(BUILD)/animation_test
clean:
	rm -rf $(BUILD)
-include $(OBJECTS:.o=.d)
.PHONY: all install check clean

check-install: all
	$(CXX) -std=c++17 -Wall -Wextra tests/install_test.cpp -lbe -o $(BUILD)/install_test
	$(BUILD)/install_test $(HOME)/config/non-packaged/apps/AirTop/AirTop
.PHONY: check-install

check-cadence: all
	$(CXX) $(CXXFLAGS) tests/cadence_test.cpp $(BUILD)/Sampler.o $(BUILD)/HardwareSensors.o $(LDLIBS) -o $(BUILD)/cadence_test
	$(BUILD)/cadence_test
.PHONY: check-cadence

check-rendering: | $(BUILD)
	$(CXX) -std=c++17 -Wall -Wextra tests/rendering_test.cpp -lbe -o $(BUILD)/rendering_test
	$(BUILD)/rendering_test
.PHONY: check-rendering

check-panel: all
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/panel_cache_test.cpp $(BUILD)/AuxPanel.o $(BUILD)/Sampler.o $(BUILD)/HardwareSensors.o $(LDFLAGS) $(LDLIBS) -o $(BUILD)/panel_cache_test
	$(BUILD)/panel_cache_test
.PHONY: check-panel

check-process-demand: all
	$(CXX) $(CXXFLAGS) tests/process_demand_test.cpp $(BUILD)/Sampler.o $(BUILD)/HardwareSensors.o $(LDLIBS) -o $(BUILD)/process_demand_test
	$(BUILD)/process_demand_test
.PHONY: check-process-demand
