#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

#include <limits>
#include <cmath>
#include "fizmo_defines.hpp"
#include "type_traits.hpp"

namespace fizmo {
namespace constants {

// Mathematical constants
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PSI = static_cast<T>(-0.618033988749894848204586834365638117720309179805762862135448L); // Conjugate of PHI

template <typename T = double>
inline constexpr enable_if_floating_point<T> psi() noexcept { return PSI<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PHI = static_cast<T>(1.618033988749894848204586834365638117720309179805762862135448622705260462818902449707207204189391137L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> phi() noexcept { return PHI<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> EULER = static_cast<T>(2.718281828459045235360287471352662497757247093699959574966967627724076630353547594571382178525166427L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> euler() noexcept { return EULER<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PI = static_cast<T>(3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> pi() noexcept { return PI<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> EULER_MASCHERONI = static_cast<T>(0.577215664901532860606512090082402431042159335939923598805767234884867726777664670936947063291746749L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> euler_mascheroni() noexcept { return EULER_MASCHERONI<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> CATALAN = static_cast<T>(0.915965594177219015054603514932384110774149374281672134266498119621763019776254769479356512926115106L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> catalan() noexcept { return CATALAN<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> APERY = static_cast<T>(1.202056903159594285399738161511449990764986292340498881792271555341838205786313090186455873609335258L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> apery() noexcept { return APERY<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> GLAISHER = static_cast<T>(1.282427129100622636875342568869791727767688927325001192063740432354L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> glaisher() noexcept { return GLAISHER<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PLASTIC = static_cast<T>(1.324717957244746025960908854478097340042744583566992359493940888204147724431771639780539064717863311L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> plastic() noexcept { return PLASTIC<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> SILVER_RATIO = static_cast<T>(2.414213562373095048801688724209698078569671875376948073176679737990732478462107038850387534327641572L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> silver_ratio() noexcept { return SILVER_RATIO<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> GELFOND_SCHNEIDER = static_cast<T>(2.665144142690225188650297249873139848274211313714659492835979593364920446178705954867609180005196416L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> gelfond_schneider() noexcept { return GELFOND_SCHNEIDER<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> KHINCHIN = static_cast<T>(2.685452001065306445309714835481795693820382293994462953051152345557218859537152002801141174931847698L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> khinchin() noexcept { return KHINCHIN<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> FEIGENBAUM = static_cast<T>(4.669201609102990671853203820466201617258185577475768632745651343004134330211314063276676504831139L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> feigenbaum() noexcept { return FEIGENBAUM<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> OMEGA = static_cast<T>(0.5671432904097838729999686622103555497538157871865125081351310792230457930866845666932194469617522946L); /*W(1)*/

template <typename T = double>
inline constexpr enable_if_floating_point<T> omega() noexcept { return OMEGA<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> GAUSS = static_cast<T>(0.834626841674073186194187123521476729290793772751784499472124831265L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> gauss() noexcept { return GAUSS<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> CHAMPERNOWNE = static_cast<T>(0.123456789101112131415161718192021222324252627282930313233343536373839404142434445464748495051525354L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> champernowne() noexcept { return CHAMPERNOWNE<T>; }

// PI constants
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RECIPROCAL_PI = static_cast<T>(0.318309886183790671537767526745028724068919291480912897495334688117793595L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> reciprocal_pi() noexcept { return RECIPROCAL_PI<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RECIPROCAL_SQRT_PI = static_cast<T>(0.56418958354775628694807945156077258584405062932899885684408572171064246844L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> reciprocal_sqrt_pi() noexcept { return RECIPROCAL_SQRT_PI<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RECIPROCAL_TWO_PI = static_cast<T>(0.15915494309189533576888376337251436203445964574045644874766734405889679763L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> reciprocal_two_pi() noexcept { return RECIPROCAL_TWO_PI<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RECIPROCAL_PI_SQUARED = static_cast<T>(0.1013211836423377714438794632097276389043587746722465488456090318941731L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> reciprocal_pi_squared() noexcept { return RECIPROCAL_PI_SQUARED<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RECIPROCAL_SQRT_TWO_PI = static_cast<T>(0.39894228040143267793994605993438186847585863116493465766592582967065792589L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> reciprocal_sqrt_two_pi() noexcept { return RECIPROCAL_SQRT_TWO_PI<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> SQRT_PI = static_cast<T>(1.77245385090551602729816748334114518279754945612238712821380778985291128459L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> sqrt_pi() noexcept { return SQRT_PI<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> SQRT_TWO_PI = static_cast<T>(2.50662827463100050241576528481104525300698674060993831662992357634229365L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> sqrt_two_pi() noexcept { return SQRT_TWO_PI<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PI_180 = static_cast<T>(0.01745329251994329576923690768488612713442871888541725456097191440171009114L); // Radians per degree

template <typename T = double>
inline constexpr enable_if_floating_point<T> pi_180() noexcept { return PI_180<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PI_8 = static_cast<T>(0.39269908169872415480783042290993786052464617492188822762186807403847705078L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> pi_8() noexcept { return PI_8<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PI_6 = static_cast<T>(0.52359877559829887307710723054658381403286156656251763682915743205130273438L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> pi_6() noexcept { return PI_6<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PI_5 = static_cast<T>(0.62831853071795864769252867665590057683943387987502116419498891846156328125L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> pi_5() noexcept { return PI_5<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PI_4 = static_cast<T>(0.78539816339744830961566084581987572104929234984377645524373614807695410157L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> pi_4() noexcept { return PI_4<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PI_3 = static_cast<T>(1.04719755119659774615421446109316762806572313312503527365831486410260547L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> pi_3() noexcept { return PI_3<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PI_2 = static_cast<T>(1.57079632679489661923132169163975144209858469968755291048747229615390820314L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> pi_2() noexcept { return PI_2<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RECIPROCAL_PI_180 = static_cast<T>(57.2957795130823208767981548141051703324054724665643215491602438612028471483L); // Degrees per radian

template <typename T = double>
inline constexpr enable_if_floating_point<T> reciprocal_pi_180() noexcept { return RECIPROCAL_PI_180<T>; }

// E constants
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RECIPROCAL_E = static_cast<T>(0.36787944117144232159552377016146086744581113103176783450783680169746149574L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> reciprocal_e() noexcept { return RECIPROCAL_E<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> SQRT_E = static_cast<T>(1.64872127070012814684865078781416357165377610071014801157507931164066102119L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> sqrt_e() noexcept { return SQRT_E<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RECIPROCAL_SQRT_E = static_cast<T>(0.60653065971263342360379953499118045344191813548718695568289215873505651941L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> reciprocal_sqrt_e() noexcept { return RECIPROCAL_SQRT_E<T>; }

// Sqrt 2 constants
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> SQRT_2 = static_cast<T>(1.41421356237309504880168872420969807856967187537694807317667973799073247846L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> sqrt_2() noexcept { return SQRT_2<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RECIPROCAL_SQRT_2 = static_cast<T>(0.5) * SQRT_2<T>;

template <typename T = double>
inline constexpr enable_if_floating_point<T> reciprocal_sqrt_2() noexcept { return RECIPROCAL_SQRT_2<T>; }

// Physical constants - Fundamental
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PLANCK_TIME = static_cast<T>(5.391e-44L); /*Seconds*/

template <typename T = long double>
inline constexpr enable_if_floating_point<T> planck_time() noexcept { return PLANCK_TIME<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PLANCK_LENGTH = static_cast<T>(1.616228369772e-35L); /*meters*/

template <typename T = long double>
inline constexpr enable_if_floating_point<T> planck_length() noexcept { return PLANCK_LENGTH<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> REDUCED_PLANCK_CONSTANT = static_cast<T>(1.054571817646156391262428003302281e-34L); /*Joule seconds*/

template <typename T = long double>
inline constexpr enable_if_floating_point<T> reduced_planck_constant() noexcept { return REDUCED_PLANCK_CONSTANT<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PLANCK_CONSTANT = static_cast<T>(6.62607015e-34L); /*Joule seconds*/

template <typename T = long double>
inline constexpr enable_if_floating_point<T> planck_constant() noexcept { return PLANCK_CONSTANT<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> LIGHT_SPEED = static_cast<T>(2.99792458e8L); /*Meters per second*/

template <typename T = double>
inline constexpr enable_if_floating_point<T> light_speed() noexcept { return LIGHT_SPEED<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> C_SQUARED = static_cast<T>(8.9875517873681764e16); /*Meters squared per second squared*/

template <typename T = double>
inline constexpr enable_if_floating_point<T> c_squared() noexcept { return C_SQUARED<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RECIPROCAL_C = static_cast<T>(3.33564095198152049584748688742058083775052744479605593141968711279332637786865234375e-9); 

template <typename T = double>
inline constexpr enable_if_floating_point<T> reciprocal_c() noexcept { return RECIPROCAL_C<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RECIPROCAL_C_SQUARED = static_cast<T>(1.1126500560536184816933562961888729071407772022491744734508920845750257200279520475305616855621337890625e-17); 

template <typename T = long double>
inline constexpr enable_if_floating_point<T> reciprocal_c_squared() noexcept { return RECIPROCAL_C_SQUARED<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> LIGHT_SECOND = static_cast<T>(2.99792458e8L); /*Meters per second*/

template <typename T = double>
inline constexpr enable_if_floating_point<T> light_second() noexcept { return LIGHT_SECOND<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> UNIVERSAL_GRAVITATIONAL = static_cast<T>(6.6743e-11L); /*Meters cubed per kilogram second squared*/

template <typename T = double>
inline constexpr enable_if_floating_point<T> universal_gravitation() noexcept { return UNIVERSAL_GRAVITATIONAL<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> GRAVITY = static_cast<T>(9.80665L); /* meters per second squared */

template <typename T = double>
inline constexpr enable_if_floating_point<T> gravity() noexcept { return GRAVITY<T>; }

// Particle masses
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> ELECTRON_MASS = static_cast<T>(9.1093837015e-31L); /* Kilograms */

template <typename T = long double>
inline constexpr enable_if_floating_point<T> electron_mass() noexcept { return ELECTRON_MASS<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> ELECTRON_MASS_AMU = static_cast<T>(5.48579905e-4L); 

template <typename T = double>
inline constexpr enable_if_floating_point<T> electron_mass_amu() noexcept { return ELECTRON_MASS_AMU<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> MUON_MASS = static_cast<T>(1.883531627e-28L); /* Kilograms */

template <typename T = long double>
inline constexpr enable_if_floating_point<T> muon_mass() noexcept { return MUON_MASS<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> TAU_MASS = static_cast<T>(3.16747e-27L); /* Kilograms */

template <typename T = long double>
inline constexpr enable_if_floating_point<T> tau_mass() noexcept { return TAU_MASS<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> ATOMIC_MASS_UNIT = static_cast<T>(1.660539067e-27L); /* Kilograms */

template <typename T = long double>
inline constexpr enable_if_floating_point<T> atomic_mass_unit() noexcept { return ATOMIC_MASS_UNIT<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PROTON_MASS = static_cast<T>(1.67262192369e-27L); /* Kilograms */

template <typename T = long double>
inline constexpr enable_if_floating_point<T> proton_mass() noexcept { return PROTON_MASS<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PROTON_MASS_AMU = static_cast<T>(1.00727646L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> proton_mass_amu() noexcept { return PROTON_MASS_AMU<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> NEUTRON_MASS = static_cast<T>(1.67492749804e-27L); /* Kilograms */

template <typename T = long double>
inline constexpr enable_if_floating_point<T> neutron_mass() noexcept { return NEUTRON_MASS<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> NEUTRON_MASS_AMU = static_cast<T>(1.00866491L); 

template <typename T = double>
inline constexpr enable_if_floating_point<T> neutron_mass_amu() noexcept { return NEUTRON_MASS_AMU<T>; }

// Electromagnetic constants
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> ELEMENTARY_CHARGE = static_cast<T>(1.602176634e-19L); /* Coulombs */

template <typename T = long double>
inline constexpr enable_if_floating_point<T> elementary_charge() noexcept { return ELEMENTARY_CHARGE<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> ELECTRIC_CONSTANT = static_cast<T>(8.8541878188e-12L); /* Farads per meter */

template <typename T = double>
inline constexpr enable_if_floating_point<T> electric_constant() noexcept { return ELECTRIC_CONSTANT<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> MAGNETIC_CONSTANT = static_cast<T>(1.25663706212e-6L); /* Newtons per ampere squared */

template <typename T = double>
inline constexpr enable_if_floating_point<T> magnetic() noexcept { return MAGNETIC_CONSTANT<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> VACUUM_IMPEDANCE = static_cast<T>(376.730313668L); /* Ohms */

template <typename T = double>
inline constexpr enable_if_floating_point<T> vacuum_impedance() noexcept { return VACUUM_IMPEDANCE<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> COULOMB = static_cast<T>(8.98755179e9L); /*Newton square meters per coloumb squared*/

template <typename T = double>
inline constexpr enable_if_floating_point<T> coulomb() noexcept { return COULOMB<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> FINE_STRUCTURE = static_cast<T>(7.29735257e-3L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> fine_structure() noexcept { return FINE_STRUCTURE<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PLANCK_MASS = static_cast<T>(2.176434e-8L); /* Kg */

template <typename T = double>
inline constexpr enable_if_floating_point<T> planck_mass() noexcept { return PLANCK_MASS<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PLANCK_CHARGE = static_cast<T>(1.8755446057566686747000987792363373157723431902313210622894257745298e-18L); /* Coulombs */

template <typename T = long double>
inline constexpr enable_if_floating_point<T> planck_charge() noexcept { return PLANCK_CHARGE<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PLANCK_VOLTAGE = static_cast<T>(1.042955e27L); /* Volts */

template <typename T = double>
inline constexpr enable_if_floating_point<T> planck_voltage() noexcept { return PLANCK_VOLTAGE<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PLANCK_CURRENT = static_cast<T>(3.4789e25L); /* Amperes */

template <typename T = double>
inline constexpr enable_if_floating_point<T> planck_current() noexcept { return PLANCK_CURRENT<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PLANCK_IMPEDANCE = static_cast<T>(376.730313668L); /* Ohms */

template <typename T = double>
inline constexpr enable_if_floating_point<T> planck_impedance() noexcept { return PLANCK_IMPEDANCE<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PLANCK_ENERGY = static_cast<T>(1.95609e9L); /* Joules */

template <typename T = double>
inline constexpr enable_if_floating_point<T> planck_energy() noexcept { return PLANCK_ENERGY<T>; }

// ESU (Electrostatic Unit) conversion constants
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> ESU_CHARGE = static_cast<T>(3.335640951981520495755767144749185e-10L); /* statcoulomb to coulomb */

template <typename T = double>
inline constexpr enable_if_floating_point<T> esu_charge() noexcept { return ESU_CHARGE<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> ESU_CURRENT = static_cast<T>(3.335640951981520495755767144749185e-10L); /* statampere to ampere */

template <typename T = double>
inline constexpr enable_if_floating_point<T> esu_current() noexcept { return ESU_CURRENT<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> ESU_VOLTAGE = static_cast<T>(299.792458L); /* statvolt to volt */

template <typename T = double>
inline constexpr enable_if_floating_point<T> esu_voltage() noexcept { return ESU_VOLTAGE<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> ESU_RESISTANCE = static_cast<T>(8.9875517873681764e11L); /* statohm to ohm */

template <typename T = double>
inline constexpr enable_if_floating_point<T> esu_resistance() noexcept { return ESU_RESISTANCE<T>; }

// Energy constants
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> ELECTRON_VOLT = static_cast<T>(1.602176634e-19L); /* Joules */

template <typename T = long double>
inline constexpr enable_if_floating_point<T> electronvolt() noexcept { return ELECTRON_VOLT<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> REDUCED_PLANCK_CONSTANT_EV = static_cast<T>(6.582119569509065698073512182441931e-16L); // eV * s

template <typename T = long double>
inline constexpr enable_if_floating_point<T> reduced_planck_constant_ev() noexcept { return REDUCED_PLANCK_CONSTANT_EV<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PLANCK_CONSTANT_EV = static_cast<T>(4.135667696923858646162230824294995e-15L); // eV * s

template <typename T = double>
inline constexpr enable_if_floating_point<T> planck_constant_ev() noexcept { return PLANCK_CONSTANT_EV<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RYDBERG_JOULE = static_cast<T>(2.179872325e-18L); /* J */

template <typename T = long double>
inline constexpr enable_if_floating_point<T> rydberg_joule() noexcept { return RYDBERG_JOULE<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RYDBERG_EV = static_cast<T>(13.605693122994L); /* eV */

template <typename T = double>
inline constexpr enable_if_floating_point<T> rydberg_ev() noexcept { return RYDBERG_EV<T>; }

// Atomic constants
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> BOHR_RADIUS = static_cast<T>(5.29177210903e-11L); /* Meters */

template <typename T = double>
inline constexpr enable_if_floating_point<T> bohr_radius() noexcept { return BOHR_RADIUS<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> BOHR_RADIUS_ANG = static_cast<T>(0.529177210903L); /* Angstroms */

template <typename T = double>
inline constexpr enable_if_floating_point<T> bohr_radius_ang() noexcept { return BOHR_RADIUS_ANG<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PROTON_RADIUS = static_cast<T>(8.414e-16L); /* Meters */

template <typename T = long double>
inline constexpr enable_if_floating_point<T> proton_radius() noexcept { return PROTON_RADIUS<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RYDBERG = static_cast<T>(1.0973731568e7L); /* 1 / meters*/

template <typename T = double>
inline constexpr enable_if_floating_point<T> rydberg() noexcept { return RYDBERG<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RYDBERG_WAVENUMBER = static_cast<T>(109737.31568539L); /* 1 / cm */

template <typename T = double>
inline constexpr enable_if_floating_point<T> rydberg_wavenumber() noexcept { return RYDBERG_WAVENUMBER<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> RYDBERG_INV_ANG = static_cast<T>(1.0973731568e-3L); /* 1/angstrom */

template <typename T = double>
inline constexpr enable_if_floating_point<T> rydberg_inverse_ang() noexcept { return RYDBERG_INV_ANG<T>; }

// Thermodynamic constants
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> BOLTZMANN = static_cast<T>(1.380649e-23L); /*Kilogram meter squared per second squared kelvin */

template <typename T = long double>
inline constexpr enable_if_floating_point<T> boltzmann() noexcept { return BOLTZMANN<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> STEFAN_BOLTZMANN = static_cast<T>(5.67e-8L); /*Watts per meter squared kelvin to the fourth*/

template <typename T = double>
inline constexpr enable_if_floating_point<T> stefan_boltzmann() noexcept { return STEFAN_BOLTZMANN<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> WIEN_WAVELENGTH = static_cast<T>(2.897771955e-3L); /* m*K */

template <typename T = double>
inline constexpr enable_if_floating_point<T> wien_wavelength() noexcept { return WIEN_WAVELENGTH<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> WIEN_WAVELENGTH_CM = static_cast<T>(0.2897771955L); /* cm*K */

template <typename T = double>
inline constexpr enable_if_floating_point<T> wien_wavelength_cm() noexcept { return WIEN_WAVELENGTH_CM<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> WIEN_FREQUENCY = static_cast<T>(5.878926e10L); /* HZ / K */

template <typename T = double>
inline constexpr enable_if_floating_point<T> wien_frequency() noexcept { return WIEN_FREQUENCY<T>; }

// Gas constants
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> IDEAL_GAS = static_cast<T>(8.314462618L); /*Cubic meters pascal per mole kelvin*/

template <typename T = double>
inline constexpr enable_if_floating_point<T> ideal_gas() noexcept { return IDEAL_GAS<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> IDEAL_GAS_JOULES = static_cast<T>(8.314462618L); /* J/(mol*K) */

template <typename T = double>
inline constexpr enable_if_floating_point<T> ideal_gas_joules() noexcept { return IDEAL_GAS_JOULES<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> IDEAL_GAS_CAL = static_cast<T>(1.987262L); /* cal/(mol*K) */

template <typename T = double>
inline constexpr enable_if_floating_point<T> ideal_gas_cal() noexcept { return IDEAL_GAS_CAL<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> IDEAL_GAS_LATM = static_cast<T>(0.08205746L); /* L*atm/(mol*K) */

template <typename T = double>
inline constexpr enable_if_floating_point<T> ideal_gas_latm() noexcept { return IDEAL_GAS_LATM<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> IDEAL_GAS_TORR = static_cast<T>(62.363577L); /* L*torr/(mol*K) */

template <typename T = double>
inline constexpr enable_if_floating_point<T> ideal_gas_torr() noexcept { return IDEAL_GAS_TORR<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> IDEAL_GAS_CCATM = static_cast<T>(82.05746L); /* cm^3*atm/(mol*K) */

template <typename T = double>
inline constexpr enable_if_floating_point<T> ideal_gas_ccatm() noexcept { return IDEAL_GAS_CCATM<T>; }

// Molar constants
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> AVOGADRO = static_cast<T>(6.02214076e23L); 

template <typename T = double>
inline constexpr enable_if_floating_point<T> avogadro() noexcept { return AVOGADRO<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> FARADAY_CONSTANT = static_cast<T>(96485.3321233100184L); /* Coulombs per mole */

template <typename T = double>
inline constexpr enable_if_floating_point<T> faraday() noexcept { return FARADAY_CONSTANT<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> FARADAY_CONSTANT_ESU = static_cast<T>(2.892586e14L); /* esu/mol */

template <typename T = double>
inline constexpr enable_if_floating_point<T> faraday_esu() noexcept { return FARADAY_CONSTANT_ESU<T>; }

// Particle physics constants
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> STRONG_COUPLING = static_cast<T>(0.1179L); /* at Z boson mass */

template <typename T = double>
inline constexpr enable_if_floating_point<T> strong_coupling() noexcept { return STRONG_COUPLING<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> WEAK_MIXING_ANGLE = static_cast<T>(0.2223L); /* Weinberg angle, dimensionless */

template <typename T = double>
inline constexpr enable_if_floating_point<T> weak_mixing_angle() noexcept { return WEAK_MIXING_ANGLE<T>; }

// Cosmological constants
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> HUBBLE = static_cast<T>(2.18e-18L); /* 1/s */

template <typename T = long double>
inline constexpr enable_if_floating_point<T> hubble() noexcept { return HUBBLE<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> HUBBLE_CONSTANT = static_cast<T>(67.4L); /* Kilometers per second per megaparsec */

template <typename T = double>
inline constexpr enable_if_floating_point<T> hubble_constant() noexcept { return HUBBLE_CONSTANT<T>; }

// Distance constants
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> PARSEC = static_cast<T>(3.085677581491367278913937957796472e16L); /*Meters*/

template <typename T = double>
inline constexpr enable_if_floating_point<T> parsec() noexcept { return PARSEC<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> ASTRONOMICAL_UNIT = static_cast<T>(1.495978707e11L); /*Meters*/

template <typename T = double>
inline constexpr enable_if_floating_point<T> astronomical_unit() noexcept { return ASTRONOMICAL_UNIT<T>; }

// Gaussian unit constants
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_arithmetic<T> MAGNETIC_CONSTANT_GAUSS = static_cast<T>(1); /* Gauss/Oersted */

template <typename T = double>
inline constexpr enable_if_floating_point<T> magnetic_constant_gauss() noexcept { return MAGNETIC_CONSTANT_GAUSS<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_arithmetic<T> MB_GAUSS = static_cast<T>(1); /*Gaussian units*/

template <typename T = double>
inline constexpr enable_if_floating_point<T> mb_gauss() noexcept { return MB_GAUSS<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> VACUUM_IMPEDANCE_GAUSSIAN = static_cast<T>(12.5663706144L); /* Gaussian units */

template <typename T = double>
inline constexpr enable_if_floating_point<T> vacuum_impedance_gaussian() noexcept { return VACUUM_IMPEDANCE_GAUSSIAN<T>; }

// Epsilon values for numerical precision
template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> TINY_EPSILON = static_cast<T>(1e-20L);

template <typename T = long double>
inline constexpr enable_if_floating_point<T> tiny_epsilon() noexcept { return TINY_EPSILON<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> FINE_EPSILON = static_cast<T>(1e-18L);

template <typename T = long double>
inline constexpr enable_if_floating_point<T> fine_epsilon() noexcept { return FINE_EPSILON<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> MIDDLE_EPSILON = static_cast<T>(1e-15L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> middle_epsilon() noexcept { return MIDDLE_EPSILON<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> EPSILON = static_cast<T>(1e-10L);

template <typename T = double>
inline constexpr enable_if_floating_point<T> epsilon() noexcept { return EPSILON<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE constexpr enable_if_floating_point<T> GENERAL_EPSILON = static_cast<T>(3.162277660168379331998893544432718533719555139352168268575049e-8L); // sqrt of middle epsilon

template <typename T = double>
inline constexpr enable_if_floating_point<T> general_epsilon() noexcept { return GENERAL_EPSILON<T>; }

// Numeric limits as templates
template <typename T>
OPTIONAL_CPP17_INLINE OPTIONAL_CPP14_CONSTEXPR enable_if_floating_point<T> POSITIVE_INFINITY = std::numeric_limits<T>::infinity();

template <typename T = double>
inline OPTIONAL_CPP14_CONSTEXPR enable_if_floating_point<T> positive_infinity() noexcept { return POSITIVE_INFINITY<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE OPTIONAL_CPP14_CONSTEXPR enable_if_floating_point<T> NEGATIVE_INFINITY = -std::numeric_limits<T>::infinity();

template <typename T = double>
inline OPTIONAL_CPP14_CONSTEXPR enable_if_floating_point<T> negative_infinity() noexcept { return NEGATIVE_INFINITY<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE OPTIONAL_CPP14_CONSTEXPR enable_if_floating_point<T> QUIET_NAN = std::numeric_limits<T>::quiet_NaN();

template <typename T = double>
inline OPTIONAL_CPP14_CONSTEXPR enable_if_floating_point<T> quiet_nan() noexcept { return QUIET_NAN<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE OPTIONAL_CPP14_CONSTEXPR enable_if_floating_point<T> SIGNALING_NAN = std::numeric_limits<T>::signaling_NaN();

template <typename T = double>
inline OPTIONAL_CPP14_CONSTEXPR enable_if_floating_point<T> signaling_nan() noexcept { return SIGNALING_NAN<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE OPTIONAL_CPP14_CONSTEXPR enable_if_arithmetic<T> TYPE_MINIMUM = std::numeric_limits<T>::lowest();

template <typename T = double>
inline OPTIONAL_CPP14_CONSTEXPR enable_if_arithmetic<T> type_minimum() noexcept { return TYPE_MINIMUM<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE OPTIONAL_CPP14_CONSTEXPR enable_if_arithmetic<T> TYPE_MAXIMUM = std::numeric_limits<T>::max();

template <typename T = double>
inline OPTIONAL_CPP14_CONSTEXPR enable_if_arithmetic<T> type_maximum() noexcept { return TYPE_MAXIMUM<T>; }

template <typename T>
OPTIONAL_CPP17_INLINE OPTIONAL_CPP14_CONSTEXPR enable_if_floating_point<T> TYPE_EPSILON = std::numeric_limits<T>::epsilon();

template <typename T = double>
inline OPTIONAL_CPP14_CONSTEXPR enable_if_floating_point<T> type_epsilon() noexcept { return TYPE_EPSILON<T>; }

} //Namespace constants
} //Namespace fizmo

#endif