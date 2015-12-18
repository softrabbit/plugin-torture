OBJS :=	src/ladspa_plugin.o src/main.o src/plugin.o src/log.o src/input_buffers.o src/lv2_plugin.o src/tests.o src/input_profile.o

CXXFLAGS = -g -msse3 -mfpmath=sse `pkg-config --cflags lilv-0`

plugin-torture:	$(OBJS)
		g++ $(CXXFLAGS) -o $@ $(OBJS) -ldl -lrt `pkg-config --libs lilv-0`

%.o:	%.cc
	gcc $(CXXFLAGS) -g -Wall -c -o $@ $<

clean:;	rm -f src/*.o plugin-torture
