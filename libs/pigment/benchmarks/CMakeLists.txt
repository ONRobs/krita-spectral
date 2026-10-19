########### next target ###############
set(ko_colorspaces_benchmark_SRCS KoColorSpacesBenchmark.cpp)
krita_add_benchmark(KoColorSpacesBenchmark TESTNAME pigment-benchmarks-KoColorSpacesBenchmark ${ko_colorspaces_benchmark_SRCS})
target_link_libraries(KoColorSpacesBenchmark kritapigment KF${KF_MAJOR}::I18n  kritatestsdk)

set(ko_compositeops_benchmark_SRCS KoCompositeOpsBenchmark.cpp)
krita_add_benchmark(KoCompositeOpsBenchmark TESTNAME pigment-benchmarks-KoCompositeOpsBenchmark ${ko_compositeops_benchmark_SRCS})
target_link_libraries(KoCompositeOpsBenchmark  kritapigment KF${KF_MAJOR}::I18n  kritatestsdk)


# krita-spectral
set(ko_spectral_compositeop_benchmark_SRCS KoSpectralCompositeOpBenchmark.cpp)
krita_add_benchmark(KoSpectralCompositeOpBenchmark TESTNAME pigment-benchmarks-KoSpectralCompositeOpBenchmark ${ko_spectral_compositeop_benchmark_SRCS})
target_link_libraries(KoSpectralCompositeOpBenchmark kritapigment KF${KF_MAJOR}::I18n kritatestsdk)
