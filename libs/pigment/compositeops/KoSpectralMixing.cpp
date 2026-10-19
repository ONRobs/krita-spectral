/*
 *  SPDX-FileCopyrightText: 2025 Ronald van Wijnen <ronald.van.wijnen@gmail.com>
 *  SPDX-FileCopyrightText: 2026 Olivers Nils Robs <olivers.robs@gmail.com>
 *
 *  SPDX-License-Identifier: MIT
 */

#include "KoSpectralMixing.h"

#include <algorithm>
#include <cmath>

namespace KoSpectral
{

namespace
{

// The data below is taken verbatim from spectral.js v3
// https://github.com/rvanwijnen/spectral.js

// Base reflectance spectra: white, cyan, magenta, yellow, red, green, blue
static const float BASE_SPECTRA[7][SIZE] = {
    { // W
        1.00116072718764f, 1.00116065159728f, 1.00116031922747f, 1.00115867270789f,
        1.00115259844552f, 1.00113252528998f, 1.00108500663327f, 1.00099687889453f,
        1.00086525152274f, 1.0006962900094f, 1.00050496114888f, 1.00030808187992f,
        1.00011966602013f, 0.999952765968407f, 0.999821836899297f, 0.999738609557593f,
        0.999709551639612f, 0.999731930210627f, 0.999799436346195f, 0.999900330316671f,
        1.00002040652611f, 1.00014478793658f, 1.00025997903412f, 1.00035579697089f,
        1.00042753780269f, 1.00047623344888f, 1.00050720967508f, 1.00052519156373f,
        1.00053509606896f, 1.00054022097482f, 1.00054272816784f, 1.00054389569087f,
        1.00054448212151f, 1.00054476959992f, 1.00054489887762f, 1.00054496254689f,
        1.00054498927058f, 1.000544996993f,
    },
    { // C
        0.970585001322962f, 0.970592498143425f, 0.970625348729891f, 0.970786806119017f,
        0.971368673228248f, 0.973163230621252f, 0.976740223158765f, 0.981587605491377f,
        0.986280265652949f, 0.989949147689134f, 0.99249270153842f, 0.994145680405256f,
        0.995183975033212f, 0.995756750110818f, 0.99591281828671f, 0.995606157834528f,
        0.994597600961854f, 0.99221571549237f, 0.986236452783249f, 0.967943337264541f,
        0.891285004244943f, 0.536202477862053f, 0.154108119001878f, 0.0574575093228929f,
        0.0315349873107007f, 0.0222633920086335f, 0.0182022841492439f, 0.016299055973264f,
        0.0153656239334613f, 0.0149111568733976f, 0.0146954339898235f, 0.0145964146717719f,
        0.0145470156699655f, 0.0145228771899495f, 0.0145120341118965f, 0.0145066940939832f,
        0.0145044507314479f, 0.0145038009464639f,
    },
    { // M
        0.990673557319988f, 0.990671524961979f, 0.990662582353421f, 0.990618107644795f,
        0.99045148087871f, 0.989871081400204f, 0.98828660875964f, 0.984290692797504f,
        0.973934905625306f, 0.941817838460145f, 0.817390326195156f, 0.432472805065729f,
        0.13845397825887f, 0.0537347216940033f, 0.0292174996673231f, 0.021313651750859f,
        0.0201349530181136f, 0.0241323096280662f, 0.0372236145223627f, 0.0760506552706601f,
        0.205375471942399f, 0.541268903460439f, 0.815841685086486f, 0.912817704123976f,
        0.946339830166962f, 0.959927696331991f, 0.966260595230312f, 0.969325970058424f,
        0.970854536721399f, 0.971605066528128f, 0.971962769757392f, 0.972127272274509f,
        0.972209417745812f, 0.972249577678424f, 0.972267621998742f, 0.97227650946215f,
        0.972280243306874f, 0.97228132482656f,
    },
    { // Y
        0.0210523371789306f, 0.0210564627517414f, 0.0210746178695038f, 0.0211649058448753f,
        0.0215027957272504f, 0.0226738799041561f, 0.0258235649693629f, 0.0334879385639851f,
        0.0519069663740307f, 0.100749014833473f, 0.239129899706847f, 0.534804312272748f,
        0.79780757864303f, 0.911449894067384f, 0.953797963004507f, 0.971241615465429f,
        0.979303123807588f, 0.983380119507575f, 0.985461246567755f, 0.986435046976605f,
        0.986738250670141f, 0.986617882445032f, 0.986277776758643f, 0.985860592444056f,
        0.98547492767621f, 0.985176934765558f, 0.984971574014181f, 0.984846303415712f,
        0.984775351811199f, 0.984738066625265f, 0.984719648311765f, 0.984711023391939f,
        0.984706683300676f, 0.984704554393091f, 0.98470359630937f, 0.984703124077552f,
        0.98470292561509f, 0.984702868122795f,
    },
    { // R
        0.0315605737777207f, 0.0315520718330149f, 0.0315148215513658f, 0.0313318044982702f,
        0.0306729857725527f, 0.0286480476989607f, 0.0246450407045709f, 0.0192960753663651f,
        0.0142066612220556f, 0.0102942608878609f, 0.0076191460521811f, 0.005898041083542f,
        0.0048233247781713f, 0.0042298748350633f, 0.0040599171299341f, 0.0043533695594676f,
        0.0053434425970201f, 0.0076917201010463f, 0.0135969795736536f, 0.0316975442661115f,
        0.107861196355249f, 0.463812603168704f, 0.847055405272011f, 0.943185409393918f,
        0.968862150696558f, 0.978030667473603f, 0.982043643854306f, 0.983923623718707f,
        0.984845484154382f, 0.985294275814596f, 0.985507295219825f, 0.985605071539837f,
        0.985653849933578f, 0.985677685033883f, 0.985688391806122f, 0.985693664690031f,
        0.985695879848205f, 0.985696521463762f,
    },
    { // G
        0.0095560747554212f, 0.0095581580120851f, 0.0095673245444588f, 0.0096129126297349f,
        0.0097837090401843f, 0.010378622705871f, 0.0120026452378567f, 0.0160977721473922f,
        0.026706190223168f, 0.0595555440185881f, 0.186039826532826f, 0.570579820116159f,
        0.861467768400292f, 0.945879089767658f, 0.970465486474305f, 0.97841363028445f,
        0.979589031411224f, 0.975533536908632f, 0.962288755397813f, 0.92312157451312f,
        0.793434018943111f, 0.459270135902429f, 0.185574103666303f, 0.0881774959955372f,
        0.05436302287667f, 0.0406288447060719f, 0.034221520431697f, 0.0311185790956966f,
        0.0295708898336134f, 0.0288108739348928f, 0.0284486271324597f, 0.0282820301724731f,
        0.0281988376490237f, 0.0281581655342037f, 0.0281398910216386f, 0.0281308901665811f,
        0.0281271086805816f, 0.0281260133612096f,
    },
    { // B
        0.979404752502014f, 0.97940070684313f, 0.979382903470261f, 0.979294364945594f,
        0.97896301460857f, 0.977814466694043f, 0.974724321133836f, 0.967198482343973f,
        0.949079657530575f, 0.900850128940977f, 0.76315044546224f, 0.465922171649319f,
        0.201263280451005f, 0.0877524413419623f, 0.0457176793291679f, 0.0284706050521843f,
        0.020527176756985f, 0.0165302792310211f, 0.0145135107212858f, 0.0136003508637687f,
        0.0133604258769571f, 0.013548894314568f, 0.0139594356366992f, 0.014443425575357f,
        0.0148854440621406f, 0.0152254296999746f, 0.0154592848180209f, 0.0156018026485961f,
        0.0156824871281936f, 0.0157248764360615f, 0.0157458108784121f, 0.0157556123350225f,
        0.0157605443964911f, 0.0157629637515278f, 0.0157640525629106f, 0.015764589232951f,
        0.0157648147772649f, 0.0157648801149616f,
    },
};

// CIE 1964 10-degree colour matching functions weighted by the D65 illuminant
static const float CMF[3][SIZE] = {
    { // X
        0.0000646919989576f, 0.0002194098998132f, 0.0011205743509343f, 0.0037666134117111f,
        0.011880553603799f, 0.0232864424191771f, 0.0345594181969747f, 0.0372237901162006f,
        0.0324183761091486f, 0.021233205609381f, 0.0104909907685421f, 0.0032958375797931f,
        0.0005070351633801f, 0.0009486742057141f, 0.0062737180998318f, 0.0168646241897775f,
        0.028689649025981f, 0.0426748124691731f, 0.0562547481311377f, 0.0694703972677158f,
        0.0830531516998291f, 0.0861260963002257f, 0.0904661376847769f, 0.0850038650591277f,
        0.0709066691074488f, 0.0506288916373645f, 0.035473961885264f, 0.0214682102597065f,
        0.0125164567619117f, 0.0068045816390165f, 0.0034645657946526f, 0.0014976097506959f,
        0.000769700480928f, 0.0004073680581315f, 0.0001690104031614f, 0.0000952245150365f,
        0.0000490309872958f, 0.0000199961492222f,
    },
    { // Y
        0.000001844289444f, 0.0000062053235865f, 0.0000310096046799f, 0.0001047483849269f,
        0.0003536405299538f, 0.0009514714056444f, 0.0022822631748318f, 0.004207329043473f,
        0.0066887983719014f, 0.0098883960193565f, 0.0152494514496311f, 0.0214183109449723f,
        0.0334229301575068f, 0.0513100134918512f, 0.070402083939949f, 0.0878387072603517f,
        0.0942490536184085f, 0.0979566702718931f, 0.0941521856862608f, 0.0867810237486753f,
        0.0788565338632013f, 0.0635267026203555f, 0.05374141675682f, 0.042646064357412f,
        0.0316173492792708f, 0.020885205921391f, 0.0138601101360152f, 0.0081026402038399f,
        0.004630102258803f, 0.0024913800051319f, 0.0012593033677378f, 0.000541646522168f,
        0.0002779528920067f, 0.0001471080673854f, 0.0000610327472927f, 0.0000343873229523f,
        0.0000177059860053f, 0.000007220974913f,
    },
    { // Z
        0.000305017147638f, 0.0010368066663574f, 0.0053131363323992f, 0.0179543925899536f,
        0.0570775815345485f, 0.113651618936287f, 0.17335872618355f, 0.196206575558657f,
        0.186082370706296f, 0.139950475383207f, 0.0891745294268649f, 0.0478962113517075f,
        0.0281456253957952f, 0.0161376622950514f, 0.0077591019215214f, 0.0042961483736618f,
        0.0020055092122156f, 0.0008614711098802f, 0.0003690387177652f, 0.0001914287288574f,
        0.0001495555858975f, 0.0000923109285104f, 0.0000681349182337f, 0.0000288263655696f,
        0.0000157671820553f, 0.0000039406041027f, 0.000001584012587f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f,
    },
};

// CIE XYZ (D65) to linear sRGB
static const float XYZ_TO_RGB[3][3] = {
    {3.2409699419045226f, -1.537383177570094f, -0.4986107602930034f},
    {-0.9692436362808796f, 1.8759675015077202f, 0.04155505740717559f},
    {0.05563007969699366f, -0.20397695888897652f, 1.0569715142428786f},
};

// spectral.js uses Number.EPSILON as the lowest reflectance. Single precision
// floats need a larger value, but the result doesn't depend on it much: the
// K/S of a black is ~1/(2 * EPSILON) while its luminance is ~EPSILON, so their
// product (which is what ends up in the mix) stays the same.
static constexpr float EPSILON = 1e-8f;

/**
 * The hot loops are written so that the compiler can vectorize them
 * (fixed trip counts that are a multiple of 8, no reductions over the bands
 * in a single accumulator, restrict pointers). This file is compiled with
 * -fno-math-errno and -fno-trapping-math (see CMakeLists.txt), otherwise
 * sqrt() and the comparisons can't be vectorized. On x86-64 Linux
 * additionally an AVX2/FMA version is compiled and selected at runtime.
 */
#if defined(__GNUC__) && !defined(__clang__) && defined(__x86_64__) && defined(__linux__)
#define SPECTRAL_TARGET_CLONES __attribute__((target_clones("arch=x86-64-v3", "default")))
#else
#define SPECTRAL_TARGET_CLONES
#endif

struct Tables {
    /// XYZ_TO_RGB * CMF, normalized so that a flat spectrum maps exactly
    /// to a gray; zero in the padding
    alignas(32) float reflectanceToRgb[3][PADDED_SIZE];

    /**
     * Base spectra: white, cyan, magenta, yellow, red, green, blue.
     *
     * spectral.js' white spectrum deviates from a flat line by about 0.1%,
     * which makes mixtures of grays very slightly tinted. Invisible for one
     * mix, but the smudge engine mixes the same pixels hundreds of times with
     * tiny factors, so the tint adds up. A flat white (together with the
     * normalization of reflectanceToRgb) keeps grays exactly neutral.
     */
    alignas(32) float baseSpectra[7][PADDED_SIZE];

    float baseLuminance[7]; ///< CIE Y of each base spectrum

    Tables()
    {
        for (int s = 0; s < 7; s++) {
            for (int i = 0; i < PADDED_SIZE; i++) {
                baseSpectra[s][i] = (s == 0 || i >= SIZE) ? 1.0f : BASE_SPECTRA[s][i];
            }
        }

        for (int c = 0; c < 3; c++) {
            double sum = 0.0;
            for (int i = 0; i < PADDED_SIZE; i++) {
                reflectanceToRgb[c][i] = i >= SIZE ? 0.0f :
                    XYZ_TO_RGB[c][0] * CMF[0][i] + XYZ_TO_RGB[c][1] * CMF[1][i] + XYZ_TO_RGB[c][2] * CMF[2][i];
                sum += reflectanceToRgb[c][i];
            }

            for (int i = 0; i < PADDED_SIZE; i++) {
                reflectanceToRgb[c][i] /= float(sum);
            }
        }

        for (int s = 0; s < 7; s++) {
            float y = 0.0f;
            for (int i = 0; i < SIZE; i++) {
                y += CMF[1][i] * baseSpectra[s][i];
            }
            baseLuminance[s] = y;
        }
    }
};

const Tables &tables()
{
    static const Tables t;
    return t;
}

/**
 * Converts a reflectance curve into linear RGB. Uses 8 partial sums per
 * channel, so the compiler can vectorize it without reordering float math.
 */
inline void reflectanceToRgb(const Tables &t, const float *__restrict reflectance, float result[3])
{
    for (int c = 0; c < 3; c++) {
        float partial[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        for (int i = 0; i < PADDED_SIZE; i += 8) {
            for (int j = 0; j < 8; j++) {
                partial[j] += t.reflectanceToRgb[c][i + j] * reflectance[i + j];
            }
        }
        result[c] = ((partial[0] + partial[1]) + (partial[2] + partial[3])) +
                    ((partial[4] + partial[5]) + (partial[6] + partial[7]));
    }
}

SPECTRAL_TARGET_CLONES
void prepareImpl(const Tables &t, const float *__restrict white, const float *__restrict secondary, const float *__restrict primary,
                 float w, float secondaryWeight, float primaryWeight,
                 float *__restrict ks, float roundTrip[3])
{
    alignas(32) float back[PADDED_SIZE];

    // plain conditionals instead of std::min/max, which return references and
    // keep the compiler from vectorizing the loop
    for (int i = 0; i < PADDED_SIZE; i++) {
        const float sum = w * white[i] + secondaryWeight * secondary[i] + primaryWeight * primary[i];
        const float reflectance = sum > EPSILON ? sum : EPSILON;
        const float a = 1.0f - reflectance;
        ks[i] = a * a / (2.0f * reflectance);
        // KM(KS(R)) == min(R, 1/R), so the round trip needs no sqrt
        const float inverse = 1.0f / reflectance;
        back[i] = reflectance < inverse ? reflectance : inverse;
    }

    reflectanceToRgb(t, back, roundTrip);
}

SPECTRAL_TARGET_CLONES
void mixImpl(const Tables &t, const float *__restrict ksA, float weightA, const float *__restrict ksB, float weightB, float mixed[3])
{
    alignas(32) float reflectance[PADDED_SIZE];

    for (int i = 0; i < PADDED_SIZE; i++) {
        const float ks = ksA[i] * weightA + ksB[i] * weightB;
        // == KM(KS) == 1 + KS - sqrt(KS^2 + 2KS), but without the cancellation
        reflectance[i] = 1.0f / (1.0f + ks + std::sqrt(ks * (ks + 2.0f)));
    }

    reflectanceToRgb(t, reflectance, mixed);
}

}

void prepare(const float linearRgb[3], Pigment &pigment)
{
    const Tables &t = tables();

    const float r0 = std::clamp(linearRgb[0], 0.0f, 1.0f);
    const float g0 = std::clamp(linearRgb[1], 0.0f, 1.0f);
    const float b0 = std::clamp(linearRgb[2], 0.0f, 1.0f);

    // decompose into white + one secondary + one primary, as spectral.js does;
    // of the six other base spectra at most one secondary and one primary
    // have a non-zero weight
    const float w = std::min({r0, g0, b0});
    const float r1 = r0 - w;
    const float g1 = g0 - w;
    const float b1 = b0 - w;

    int secondary = 1; // cyan
    float secondaryWeight = std::min(g1, b1);
    if (std::min(r1, b1) > secondaryWeight) {
        secondary = 2; // magenta
        secondaryWeight = std::min(r1, b1);
    }
    if (std::min(r1, g1) > secondaryWeight) {
        secondary = 3; // yellow
        secondaryWeight = std::min(r1, g1);
    }

    const float primaryWeights[3] = {
        std::max(0.0f, std::min(r1 - b1, r1 - g1)), // red
        std::max(0.0f, std::min(g1 - b1, g1 - r1)), // green
        std::max(0.0f, std::min(b1 - g1, b1 - r1)), // blue
    };
    const int primaryIndex = int(std::max_element(primaryWeights, primaryWeights + 3) - primaryWeights);
    const int primary = 4 + primaryIndex;
    const float primaryWeight = primaryWeights[primaryIndex];

    pigment.luminance = std::max(EPSILON, w * t.baseLuminance[0] +
                                          secondaryWeight * t.baseLuminance[secondary] +
                                          primaryWeight * t.baseLuminance[primary]);

    float roundTrip[3];
    prepareImpl(t, t.baseSpectra[0], t.baseSpectra[secondary], t.baseSpectra[primary],
                w, secondaryWeight, primaryWeight, pigment.ks, roundTrip);

    for (int c = 0; c < 3; c++) {
        pigment.roundTripError[c] = roundTrip[c] - linearRgb[c];
    }
}

void mix(const Pigment &a, const float aLinearRgb[3],
         const Pigment &b, const float bLinearRgb[3],
         float factor, float result[3])
{
    if (factor <= 0.0f) {
        std::copy(aLinearRgb, aLinearRgb + 3, result);
        return;
    }

    if (factor >= 1.0f) {
        std::copy(bLinearRgb, bLinearRgb + 3, result);
        return;
    }

    const float invFactor = 1.0f - factor;
    const float concentrationA = invFactor * invFactor * a.luminance;
    const float concentrationB = factor * factor * b.luminance;
    const float normalization = 1.0f / (concentrationA + concentrationB);

    float mixed[3];
    mixImpl(tables(), a.ks, concentrationA * normalization, b.ks, concentrationB * normalization, mixed);

    for (int c = 0; c < 3; c++) {
        result[c] = mixed[c] - (invFactor * a.roundTripError[c] + factor * b.roundTripError[c]);
    }
}

void mix(const float aLinearRgb[3], const float bLinearRgb[3], float factor, float result[3])
{
    Pigment a;
    Pigment b;
    prepare(aLinearRgb, a);
    prepare(bLinearRgb, b);
    mix(a, aLinearRgb, b, bLinearRgb, factor, result);
}

}
