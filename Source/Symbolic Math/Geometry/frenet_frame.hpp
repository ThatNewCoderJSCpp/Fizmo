#ifndef FIZMO_FRENET_FRAME_HPP
#define FIZMO_FRENET_FRAME_HPP

#include <string>
#include "../Linalg/vectors.hpp"
#include "../Linalg/matrices.hpp"
#include "../../Vectors/vectors.hpp"
#include <memory>

namespace fizmo {
namespace math {
namespace geometry {

struct EvaluatedFrenetFrame {
    Vector3D<double, double> T;
    Vector3D<double, double> N;
    Vector3D<double, double> B;
    double kappa;
    double tau;

    friend std::ostream& operator<<(std::ostream& os, const EvaluatedFrenetFrame& frame) {
        os << "Frenet Frame:\n"
           << "    T: " << frame.T << "\n"
           << "    N: " << frame.N << "\n"
           << "    B: " << frame.B << "\n"
           << "    \u03ba: " << frame.kappa << "\n"
           << "    \u03c4: " << frame.tau;
        return os;
    }

    std::string to_string() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }
};

class FrenetFrame {
public:
    FrenetFrame(vectors::SymbolicVector3 T, vectors::SymbolicVector3 N, vectors::SymbolicVector3 B, cas::Expression kappa, cas::Expression tau, std::string param)
;

    const vectors::SymbolicVector3& T()     const { return T_;     }
    const vectors::SymbolicVector3& N()     const { return N_;     }
    const vectors::SymbolicVector3& B()     const { return B_;     }
    const cas::Expression&      kappa() const { return kappa_; }
    const cas::Expression&      tau()   const { return tau_;   }
    Vector3D<double, double> T_at(double t)     const { return T_.evaluate({{param_, t}}); }
    Vector3D<double, double> N_at(double t)     const { return N_.evaluate({{param_, t}}); }
    Vector3D<double, double> B_at(double t)     const { return B_.evaluate({{param_, t}}); }
    double kappa_at(double t) const { return kappa_.evaluate({{param_, t}}); }
    double tau_at(double t)   const { return tau_.evaluate({{param_, t}}); }
    EvaluatedFrenetFrame evaluate_at(double t) const { return { T_at(t), N_at(t), B_at(t), kappa_at(t), tau_at(t) }; }

    friend std::ostream& operator<<(std::ostream& os, const FrenetFrame& frame) {
        os << "Frenet Frame:\n"
           << "    T: " << frame.T_ << "\n"
           << "    N: " << frame.N_ << "\n"
           << "    B: " << frame.B_ << "\n"
           << "    \u03ba: " << frame.kappa_.full_simplify() << "\n"
           << "    \u03c4: " << frame.tau_.full_simplify();
        return os;
    }

    std::string to_string() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

private:
    vectors::SymbolicVector3 T_, N_, B_;
    cas::Expression      kappa_, tau_;
    std::string     param_;
};

struct EvaluatedOsculatingCircle {
    Vector3D<double, double> center;
    double                   radius;
    Vector3D<double, double> T;
    Vector3D<double, double> N;

    friend std::ostream& operator<<(std::ostream& os, const EvaluatedOsculatingCircle& frame) {
        os << "Osculating Circle:\n"
           << "    T: " << frame.T << "\n"
           << "    N: " << frame.N << "\n"
           << "    Center: " << frame.center << "\n"
           << "    Radius: " << frame.radius;
        return os;
    }

    std::string to_string() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }
};

class OsculatingCircle {
public:
    OsculatingCircle(vectors::SymbolicVector3 center, cas::Expression radius, vectors::SymbolicVector3 T, vectors::SymbolicVector3 N, std::string param)
;

    const vectors::SymbolicVector3& center() const { return center_; }
    const cas::Expression&      radius() const { return radius_; }
    const vectors::SymbolicVector3& T()      const { return T_; }
    const vectors::SymbolicVector3& N()      const { return N_; }
    Vector3D<double, double> center_at(double t) const { return center_.evaluate({{param_, t}}); }
    double                   radius_at(double t) const { return radius_.evaluate({{param_, t}}); }

    EvaluatedOsculatingCircle evaluate_at(double t) const {
        return {
            center_at(t),
            radius_at(t),
            T_.evaluate({{param_, t}}),
            N_.evaluate({{param_, t}})
        };
    }

    friend std::ostream& operator<<(std::ostream& os, const OsculatingCircle& frame) {
        os << "Osculating Circle:\n"
           << "    T: " << frame.T_ << "\n"
           << "    N: " << frame.N_ << "\n"
           << "    Center: " << frame.center_ << "\n"
           << "    Radius: " << frame.radius_.full_simplify();
        return os;
    }

    std::string to_string() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

private:
    vectors::SymbolicVector3 center_, T_, N_;
    cas::Expression      radius_;
    std::string     param_;
};

struct EvaluatedFrenetFrame2D {
    Vector2D<double, double> T;
    Vector2D<double, double> N;
    double kappa;

    friend std::ostream& operator<<(std::ostream& os, const EvaluatedFrenetFrame2D& f) {
        os << "Frenet Frame (2D):\n"
           << "    T: " << f.T << "\n"
           << "    N: " << f.N << "\n"
           << "    κ: " << f.kappa;
        return os;
    }
    std::string to_string() const { std::stringstream ss; ss << *this; return ss.str(); }
};

struct EvaluatedOsculatingCircle2D {
    Vector2D<double, double> center;
    double                   radius;
    Vector2D<double, double> T;
    Vector2D<double, double> N;

    friend std::ostream& operator<<(std::ostream& os, const EvaluatedOsculatingCircle2D& c) {
        os << "Osculating Circle (2D):\n"
           << "    T: " << c.T << "\n"
           << "    N: " << c.N << "\n"
           << "    Center: " << c.center << "\n"
           << "    Radius: " << c.radius;
        return os;
    }
    std::string to_string() const { std::stringstream ss; ss << *this; return ss.str(); }
};

struct EvaluatedFrenetFrame4D {
    vector4d T;
    vector4d N;
    double   kappa;

    friend std::ostream& operator<<(std::ostream& os, const EvaluatedFrenetFrame4D& f) {
        os << "Frenet Frame (4D):\n"
           << "    T: " << f.T << "\n"
           << "    N: " << f.N << "\n"
           << "    κ: " << f.kappa;
        return os;
    }
    std::string to_string() const { std::stringstream ss; ss << *this; return ss.str(); }
};

struct EvaluatedOsculatingCircle4D {
    vector4d center;
    double   radius;
    vector4d T;
    vector4d N;

    friend std::ostream& operator<<(std::ostream& os, const EvaluatedOsculatingCircle4D& c) {
        os << "Osculating Circle (4D):\n"
           << "    T: " << c.T << "\n"
           << "    N: " << c.N << "\n"
           << "    Center: " << c.center << "\n"
           << "    Radius: " << c.radius;
        return os;
    }
    std::string to_string() const { std::stringstream ss; ss << *this; return ss.str(); }
};

struct EvaluatedFrenetFrameN {
    std::vector<double> T;
    std::vector<double> N;
    double kappa;

    friend std::ostream& operator<<(std::ostream& os, const EvaluatedFrenetFrameN& f) {
        os << "Frenet Frame (ND):\n    T: (";
        for (std::size_t i = 0; i < f.T.size(); ++i) { if (i) os << ", "; os << f.T[i]; }
        os << ")\n    N: (";
        for (std::size_t i = 0; i < f.N.size(); ++i) { if (i) os << ", "; os << f.N[i]; }
        os << ")\n    κ: " << f.kappa;
        return os;
    }

    std::string to_string() const { std::stringstream ss; ss << *this; return ss.str(); }
};

struct EvaluatedOsculatingCircleN {
    std::vector<double> center;
    double              radius;
    std::vector<double> T;
    std::vector<double> N;

    friend std::ostream& operator<<(std::ostream& os, const EvaluatedOsculatingCircleN& c) {
        os << "Osculating Circle (ND):\n    T: (";
        for (std::size_t i = 0; i < c.T.size(); ++i) { if (i) os << ", "; os << c.T[i]; }
        os << ")\n    N: (";
        for (std::size_t i = 0; i < c.N.size(); ++i) { if (i) os << ", "; os << c.N[i]; }
        os << ")\n    Center: (";
        for (std::size_t i = 0; i < c.center.size(); ++i) { if (i) os << ", "; os << c.center[i]; }
        os << ")\n    Radius: " << c.radius;
        return os;
    }
    std::string to_string() const { std::stringstream ss; ss << *this; return ss.str(); }
};

class FrenetFrame2D {
public:
    FrenetFrame2D(vectors::SymbolicVector2 T, vectors::SymbolicVector2 N, cas::Expression kappa, std::string param) : T_(std::move(T)), N_(std::move(N)), kappa_(std::move(kappa)), param_(std::move(param)) {}

    const vectors::SymbolicVector2& T()     const { return T_;     }
    const vectors::SymbolicVector2& N()     const { return N_;     }
    const cas::Expression&      kappa() const { return kappa_; }

    Vector2D<double, double> T_at(double t)     const { return T_.evaluate({{param_, t}}); }
    Vector2D<double, double> N_at(double t)     const { return N_.evaluate({{param_, t}}); }
    double                   kappa_at(double t) const { return kappa_.evaluate({{param_, t}}); }

    EvaluatedFrenetFrame2D evaluate_at(double t) const { return { T_at(t), N_at(t), kappa_at(t) }; }

    friend std::ostream& operator<<(std::ostream& os, const FrenetFrame2D& f) {
        os << "Frenet Frame (2D):\n"
           << "    T: " << f.T_ << "\n"
           << "    N: " << f.N_ << "\n"
           << "    κ: " << f.kappa_.full_simplify();
        return os;
    }
    std::string to_string() const { std::stringstream ss; ss << *this; return ss.str(); }

private:
    vectors::SymbolicVector2 T_, N_;
    cas::Expression      kappa_;
    std::string     param_;
};

class OsculatingCircle2D {
public:
    OsculatingCircle2D(vectors::SymbolicVector2 center, cas::Expression radius, vectors::SymbolicVector2 T, vectors::SymbolicVector2 N, std::string param);

    const vectors::SymbolicVector2& center() const { return center_; }
    const cas::Expression&      radius() const { return radius_; }
    const vectors::SymbolicVector2& T()      const { return T_; }
    const vectors::SymbolicVector2& N()      const { return N_; }

    Vector2D<double, double> center_at(double t) const { return center_.evaluate({{param_, t}}); }
    double                   radius_at(double t) const { return radius_.evaluate({{param_, t}}); }

    EvaluatedOsculatingCircle2D evaluate_at(double t) const {
        return { center_at(t), radius_at(t), T_.evaluate({{param_, t}}), N_.evaluate({{param_, t}}) };
    }

    friend std::ostream& operator<<(std::ostream& os, const OsculatingCircle2D& c) {
        os << "Osculating Circle (2D):\n"
           << "    T: " << c.T_ << "\n"
           << "    N: " << c.N_ << "\n"
           << "    Center: " << c.center_ << "\n"
           << "    Radius: " << c.radius_.full_simplify();
        return os;
    }
    std::string to_string() const { std::stringstream ss; ss << *this; return ss.str(); }

private:
    vectors::SymbolicVector2 center_, T_, N_;
    cas::Expression      radius_;
    std::string     param_;
};

class FrenetFrame4D {
public:
    FrenetFrame4D(vectors::SymbolicVector4 T, vectors::SymbolicVector4 N, cas::Expression kappa, std::string param) : T_(std::move(T)), N_(std::move(N)), kappa_(std::move(kappa)), param_(std::move(param)) {}

    const vectors::SymbolicVector4& T()     const { return T_;     }
    const vectors::SymbolicVector4& N()     const { return N_;     }
    const cas::Expression&      kappa() const { return kappa_; }

    vector4d T_at(double t)     const { return T_.evaluate({{param_, t}}); }
    vector4d N_at(double t)     const { return N_.evaluate({{param_, t}}); }
    double   kappa_at(double t) const { return kappa_.evaluate({{param_, t}}); }

    EvaluatedFrenetFrame4D evaluate_at(double t) const { return { T_at(t), N_at(t), kappa_at(t) }; }

    friend std::ostream& operator<<(std::ostream& os, const FrenetFrame4D& f) {
        os << "Frenet Frame (4D):\n"
           << "    T: " << f.T_ << "\n"
           << "    N: " << f.N_ << "\n"
           << "    κ: " << f.kappa_.full_simplify();
        return os;
    }

    std::string to_string() const { std::stringstream ss; ss << *this; return ss.str(); }

private:
    vectors::SymbolicVector4 T_, N_;
    cas::Expression      kappa_;
    std::string     param_;
};

class OsculatingCircle4D {
public:
    OsculatingCircle4D(vectors::SymbolicVector4 center, cas::Expression radius, vectors::SymbolicVector4 T, vectors::SymbolicVector4 N, std::string param)
;

    const vectors::SymbolicVector4& center() const { return center_; }
    const cas::Expression&      radius() const { return radius_; }
    const vectors::SymbolicVector4& T()      const { return T_; }
    const vectors::SymbolicVector4& N()      const { return N_; }

    vector4d center_at(double t) const { return center_.evaluate({{param_, t}}); }
    double   radius_at(double t) const { return radius_.evaluate({{param_, t}}); }

    EvaluatedOsculatingCircle4D evaluate_at(double t) const {
        return { center_at(t), radius_at(t), T_.evaluate({{param_, t}}), N_.evaluate({{param_, t}}) };
    }

    friend std::ostream& operator<<(std::ostream& os, const OsculatingCircle4D& c) {
        os << "Osculating Circle (4D):\n"
           << "    T: " << c.T_ << "\n"
           << "    N: " << c.N_ << "\n"
           << "    Center: " << c.center_ << "\n"
           << "    Radius: " << c.radius_.full_simplify();
        return os;
    }

    std::string to_string() const { std::stringstream ss; ss << *this; return ss.str(); }

private:
    vectors::SymbolicVector4 center_, T_, N_;
    cas::Expression      radius_;
    std::string     param_;
};

class FrenetFrameN {
public:
    FrenetFrameN(vectors::SymbolicVectorN T, vectors::SymbolicVectorN N, cas::Expression kappa, std::string param) : T_(std::move(T)), N_(std::move(N)), kappa_(std::move(kappa)), param_(std::move(param)) {}

    const vectors::SymbolicVectorN& T()     const { return T_;     }
    const vectors::SymbolicVectorN& N()     const { return N_;     }
    const cas::Expression&      kappa() const { return kappa_; }

    std::vector<double> T_at(double t)     const { return T_.evaluate({{param_, t}}); }
    std::vector<double> N_at(double t)     const { return N_.evaluate({{param_, t}}); }
    double              kappa_at(double t) const { return kappa_.evaluate({{param_, t}}); }

    EvaluatedFrenetFrameN evaluate_at(double t) const { return { T_at(t), N_at(t), kappa_at(t) }; }

    friend std::ostream& operator<<(std::ostream& os, const FrenetFrameN& f) {
        os << "Frenet Frame (ND):\n"
           << "    T: " << f.T_ << "\n"
           << "    N: " << f.N_ << "\n"
           << "    κ: " << f.kappa_.full_simplify();
        return os;
    }
    std::string to_string() const { std::stringstream ss; ss << *this; return ss.str(); }

private:
    vectors::SymbolicVectorN T_, N_;
    cas::Expression      kappa_;
    std::string     param_;
};

class OsculatingCircleN {
public:
    OsculatingCircleN(vectors::SymbolicVectorN center, cas::Expression radius, vectors::SymbolicVectorN T, vectors::SymbolicVectorN N, std::string param)
;

    const vectors::SymbolicVectorN& center() const { return center_; }
    const cas::Expression&      radius() const { return radius_; }
    const vectors::SymbolicVectorN& T()      const { return T_; }
    const vectors::SymbolicVectorN& N()      const { return N_; }

    std::vector<double> center_at(double t) const { return center_.evaluate({{param_, t}}); }
    double              radius_at(double t) const { return radius_.evaluate({{param_, t}}); }

    EvaluatedOsculatingCircleN evaluate_at(double t) const {
        return { center_at(t), radius_at(t), T_.evaluate({{param_, t}}), N_.evaluate({{param_, t}}) };
    }

    friend std::ostream& operator<<(std::ostream& os, const OsculatingCircleN& c) {
        os << "Osculating Circle (ND):\n"
           << "    T: " << c.T_ << "\n"
           << "    N: " << c.N_ << "\n"
           << "    Center: " << c.center_ << "\n"
           << "    Radius: " << c.radius_.full_simplify();
        return os;
    }
    std::string to_string() const { std::stringstream ss; ss << *this; return ss.str(); }

private:
    vectors::SymbolicVectorN center_, T_, N_;
    cas::Expression      radius_;
    std::string     param_;
};

class ParametricCurve {
public:
    ParametricCurve(vectors::SymbolicVector3 r, std::string param = "t") : r_(std::move(r)), param_(std::move(param)) {}
    ParametricCurve(cas::Expression x_t, cas::Expression y_t, cas::Expression z_t, std::string param = "t") : r_(std::move(x_t), std::move(y_t), std::move(z_t)), param_(std::move(param)) {}
    
    const vectors::SymbolicVector3& r()     const { return r_; }
    const std::string&     param() const { return param_; }

    const vectors::SymbolicVector3& r_prime() const;

    const vectors::SymbolicVector3& r_double_prime() const;

    const vectors::SymbolicVector3& r_triple_prime() const;

    const vectors::SymbolicVector3& cross_rp_rpp() const;

    cas::Expression speed() const { return r_prime().magnitude(); }

    cas::Expression curvature() const;

    cas::Expression torsion() const;

    const vectors::SymbolicVector3& T() const {
        if (!T_) T_ = std::make_unique<vectors::SymbolicVector3>(r_prime().normalized());
        return *T_;
    }

    const vectors::SymbolicVector3& B() const;

    const vectors::SymbolicVector3& N() const {
        if (!N_) N_ = std::make_unique<vectors::SymbolicVector3>(B().cross(T()));
        return *N_;
    }

    const cas::Expression& inv_curvature() const;

    const vectors::SymbolicVector3& osculating_center() const;

    FrenetFrame frenet_frame() const { return FrenetFrame(T(), N(), B(), curvature(), torsion(), param_); }
    OsculatingCircle osculating_circle() const { return OsculatingCircle(osculating_center(), inv_curvature(), T(), N(), param_); }

    Vector3D<double, double> position_at(double t) const { return r_.evaluate({{param_, t}}); }
    Vector3D<double, double> velocity_at(double t) const { return rp_->evaluate({{param_, t}}); }
    double speed_at(double t) const { return velocity_at(t).magnitude(); }
    Vector3D<double, double> acceleration_at(double t) const { return rpp_->evaluate({{param_, t}}); }
    Vector3D<double, double> jerk_at(double t) const { return rppp_->evaluate({{param_, t}}); }

    EvaluatedFrenetFrame frenet_frame_at(double t) const { return frenet_frame().evaluate_at(t); }
    EvaluatedOsculatingCircle osculating_circle_at(double t) const { return osculating_circle().evaluate_at(t); }

    friend std::ostream& operator<<(std::ostream& os, const ParametricCurve& frame) {
        os << "Parametric curve:\n"
           << "    r: " << frame.r().full_simplify() << "\n"
           << "    r':" << frame.r_prime().full_simplify() << "\n"
           << "    r'': " << frame.r_double_prime().full_simplify() << "\n"
           << "    r''': " << frame.r_triple_prime().full_simplify() << "\n"
           << "    T: " << frame.T() << "\n"
           << "    N: " << frame.N() << "\n"
           << "    B: " << frame.B() << "\n"
           << "    \u03c4: " << frame.torsion().full_simplify() << "\n"
           << "    \u03ba: " << frame.curvature().full_simplify() << "\n"
           << "    1/\u03ba: " << frame.inv_curvature().full_simplify() << "\n"
           << "    Center: " << frame.osculating_center();
        return os;
    }

    std::string to_string() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

private:
    vectors::SymbolicVector3 r_;
    std::string     param_;
    mutable std::unique_ptr<vectors::SymbolicVector3> rp_;
    mutable std::unique_ptr<vectors::SymbolicVector3> rpp_;
    mutable std::unique_ptr<vectors::SymbolicVector3> rppp_;
    mutable std::unique_ptr<vectors::SymbolicVector3> cross_;
    mutable std::unique_ptr<vectors::SymbolicVector3> T_;
    mutable std::unique_ptr<vectors::SymbolicVector3> N_;
    mutable std::unique_ptr<vectors::SymbolicVector3> B_;
    mutable std::unique_ptr<cas::Expression>      curvature_;
    mutable std::unique_ptr<cas::Expression>      torsion_;
    mutable std::unique_ptr<cas::Expression>      inv_curvature_;
    mutable std::unique_ptr<vectors::SymbolicVector3> osculating_center_;
};

class ParametricCurve2D {
public:
    ParametricCurve2D(vectors::SymbolicVector2 r, std::string param = "t") : r_(std::move(r)), param_(std::move(param)) {}
    ParametricCurve2D(cas::Expression x_t, cas::Expression y_t, std::string param = "t") : r_(std::move(x_t), std::move(y_t)), param_(std::move(param)) {}

    const vectors::SymbolicVector2& r()     const { return r_; }
    const std::string&     param() const { return param_; }

    const vectors::SymbolicVector2& r_prime() const;

    const vectors::SymbolicVector2& r_double_prime() const;

    const vectors::SymbolicVector2& r_triple_prime() const;

    cas::Expression speed() const { return r_prime().magnitude(); }

    const cas::Expression& cross_2d() const;

    cas::Expression signed_curvature() const;

    cas::Expression curvature() const;

    const vectors::SymbolicVector2& T() const {
        if (!T_) T_ = std::make_unique<vectors::SymbolicVector2>(r_prime().normalized());
        return *T_;
    }

    const vectors::SymbolicVector2& N() const;

    const cas::Expression& inv_curvature() const;

    cas::Expression signed_radius() const { return cas::Const(1.0) / signed_curvature(); }

    const vectors::SymbolicVector2& osculating_center() const;

    FrenetFrame2D frenet_frame() const { return FrenetFrame2D(T(), N(), signed_curvature(), param_); }
    OsculatingCircle2D osculating_circle() const { return OsculatingCircle2D(osculating_center(), inv_curvature(), T(), N(), param_); }

    Vector2D<double, double> position_at(double t)     const { return r_.evaluate({{param_, t}}); }
    Vector2D<double, double> velocity_at(double t)     const { return r_prime().evaluate({{param_, t}}); }
    double                   speed_at(double t)        const { return velocity_at(t).magnitude(); }
    Vector2D<double, double> acceleration_at(double t) const { return r_double_prime().evaluate({{param_, t}}); }
    Vector2D<double, double> jerk_at(double t)         const { return r_triple_prime().evaluate({{param_, t}}); }

    EvaluatedFrenetFrame2D       frenet_frame_at(double t)       const { return frenet_frame().evaluate_at(t); }
    EvaluatedOsculatingCircle2D  osculating_circle_at(double t)  const { return osculating_circle().evaluate_at(t); }

    struct ClosureResult {
        bool                     found     = false;
        double                   parameter = 0.0;
        Vector2D<double, double> point     = { 0.0, 0.0 };
    };

    bool is_closed(double t0, double t1, double tol = constants::middle_epsilon()) const;

    ClosureResult find_closure(
        double t0, double t1,
        std::uint64_t n_samples = 256,
        double tol = constants::middle_epsilon()
    ) const;

    bool has_closure(
        double t0, double t1,
        std::uint64_t n_samples = 256,
        double tol = constants::middle_epsilon()
    ) const;

    friend std::ostream& operator<<(std::ostream& os, const ParametricCurve2D& c) {
        os << "Parametric Curve (2D):\n"
           << "    r:  " << c.r().full_simplify() << "\n"
           << "    r': " << c.r_prime().full_simplify() << "\n"
           << "    r'': " << c.r_double_prime().full_simplify() << "\n"
           << "    T: " << c.T() << "\n"
           << "    N: " << c.N() << "\n"
           << "    κ (signed): " << c.signed_curvature().full_simplify() << "\n"
           << "    κ: " << c.curvature().full_simplify() << "\n"
           << "    1/κ: " << c.inv_curvature().full_simplify() << "\n"
           << "    Center: " << c.osculating_center();
        return os;
    }

    std::string to_string() const { std::stringstream ss; ss << *this; return ss.str(); }

private:
    static double dist_squared(const Vector2D<double, double>& a, const Vector2D<double, double>& b) noexcept {
        double dx = a.x - b.x;
        double dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    ClosureResult refine_closure(
        const Vector2D<double, double>& p0,
        double a, double b,
        double tol_sq
    ) const;

private:
    vectors::SymbolicVector2 r_;
    std::string     param_;

    mutable std::unique_ptr<vectors::SymbolicVector2> rp_;
    mutable std::unique_ptr<vectors::SymbolicVector2> rpp_;
    mutable std::unique_ptr<vectors::SymbolicVector2> rppp_;
    mutable std::unique_ptr<cas::Expression>      cross2d_;
    mutable std::unique_ptr<cas::Expression>      signed_curvature_;
    mutable std::unique_ptr<cas::Expression>      curvature_;
    mutable std::unique_ptr<vectors::SymbolicVector2> T_;
    mutable std::unique_ptr<vectors::SymbolicVector2> N_;
    mutable std::unique_ptr<cas::Expression>      inv_curvature_;
    mutable std::unique_ptr<vectors::SymbolicVector2> osc_center_;
};

class ParametricCurve4D {
public:
    ParametricCurve4D(vectors::SymbolicVector4 r, std::string param = "t") : r_(std::move(r)), param_(std::move(param)) {}
    ParametricCurve4D(cas::Expression x_t, cas::Expression y_t, cas::Expression z_t, cas::Expression w_t, std::string param = "t");

    const vectors::SymbolicVector4& r()     const { return r_; }
    const std::string&     param() const { return param_; }

    const vectors::SymbolicVector4& r_prime() const;

    const vectors::SymbolicVector4& r_double_prime() const;

    const vectors::SymbolicVector4& r_triple_prime() const;

    cas::Expression speed() const { return r_prime().magnitude(); }

    cas::Expression curvature() const;

    const vectors::SymbolicVector4& T() const {
        if (!T_) T_ = std::make_unique<vectors::SymbolicVector4>(r_prime().normalized());
        return *T_;
    }

    const vectors::SymbolicVector4& N() const;

    const cas::Expression& inv_curvature() const;

    const vectors::SymbolicVector4& osculating_center() const;

    FrenetFrame4D frenet_frame() const { return FrenetFrame4D(T(), N(), curvature(), param_); }

    OsculatingCircle4D osculating_circle() const {
        return OsculatingCircle4D(osculating_center(), inv_curvature(), T(), N(), param_);
    }

    vector4d position_at(double t)     const { return r_.evaluate({{param_, t}}); }
    vector4d velocity_at(double t)     const { return r_prime().evaluate({{param_, t}}); }
    vector4d acceleration_at(double t) const { return r_double_prime().evaluate({{param_, t}}); }
    vector4d jerk_at(double t)         const { return r_triple_prime().evaluate({{param_, t}}); }

    double speed_at(double t) const {
        auto v = velocity_at(t);
        return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z + v.w*v.w);
    }

    EvaluatedFrenetFrame4D       frenet_frame_at(double t)      const { return frenet_frame().evaluate_at(t); }
    EvaluatedOsculatingCircle4D  osculating_circle_at(double t) const { return osculating_circle().evaluate_at(t); }

    friend std::ostream& operator<<(std::ostream& os, const ParametricCurve4D& c) {
        os << "Parametric Curve (4D):\n"
           << "    r:  " << c.r().full_simplify() << "\n"
           << "    r': " << c.r_prime().full_simplify() << "\n"
           << "    r'': " << c.r_double_prime().full_simplify() << "\n"
           << "    T: " << c.T() << "\n"
           << "    N: " << c.N() << "\n"
           << "    κ: " << c.curvature().full_simplify() << "\n"
           << "    1/κ: " << c.inv_curvature().full_simplify() << "\n"
           << "    Center: " << c.osculating_center();
        return os;
    }

    std::string to_string() const { std::stringstream ss; ss << *this; return ss.str(); }

private:
    vectors::SymbolicVector4 r_;
    std::string     param_;

    mutable std::unique_ptr<vectors::SymbolicVector4> rp_;
    mutable std::unique_ptr<vectors::SymbolicVector4> rpp_;
    mutable std::unique_ptr<vectors::SymbolicVector4> rppp_;
    mutable std::unique_ptr<cas::Expression>      curvature_;
    mutable std::unique_ptr<vectors::SymbolicVector4> T_;
    mutable std::unique_ptr<vectors::SymbolicVector4> N_;
    mutable std::unique_ptr<cas::Expression>      inv_curvature_;
    mutable std::unique_ptr<vectors::SymbolicVector4> osc_center_;
};

class ParametricCurveN {
public:
    ParametricCurveN(vectors::SymbolicVectorN r, std::string param) : r_(std::move(r)), param_(std::move(param)) {}
    const vectors::SymbolicVectorN& r()     const { return r_; }
    const std::string&     param() const { return param_; }
    std::size_t            dim()   const { return r_.dimension(); }

    const vectors::SymbolicVectorN& r_prime() const;

    const vectors::SymbolicVectorN& r_double_prime() const;

    const vectors::SymbolicVectorN& r_triple_prime() const;

    cas::Expression speed() const { return r_prime().magnitude(); }

    cas::Expression curvature() const;

    const vectors::SymbolicVectorN& T() const {
        if (!T_) T_ = std::make_unique<vectors::SymbolicVectorN>(r_prime().normalized());
        return *T_;
    }

    const vectors::SymbolicVectorN& N() const;

    const cas::Expression& inv_curvature() const;

    const vectors::SymbolicVectorN& osculating_center() const;

    FrenetFrameN frenet_frame() const { return FrenetFrameN(T(), N(), curvature(), param_); }
    OsculatingCircleN osculating_circle() const { return OsculatingCircleN(osculating_center(), inv_curvature(), T(), N(), param_); }

    std::vector<double> position_at(double t)     const { return r_.evaluate({{param_, t}}); }
    std::vector<double> velocity_at(double t)     const { return r_prime().evaluate({{param_, t}}); }

    double              speed_at(double t)        const {
        auto v = velocity_at(t);
        double s = 0; for (auto x : v) s += x * x;
        return std::sqrt(s);
    }

    std::vector<double> acceleration_at(double t) const { return r_double_prime().evaluate({{param_, t}}); }
    std::vector<double> jerk_at(double t)         const { return r_triple_prime().evaluate({{param_, t}}); }

    EvaluatedFrenetFrameN       frenet_frame_at(double t)      const { return frenet_frame().evaluate_at(t); }
    EvaluatedOsculatingCircleN  osculating_circle_at(double t) const { return osculating_circle().evaluate_at(t); }

    friend std::ostream& operator<<(std::ostream& os, const ParametricCurveN& c) {
        os << "Parametric Curve (" << c.dim() << "D):\n"
           << "    r:  " << c.r().full_simplify() << "\n"
           << "    r': " << c.r_prime().full_simplify() << "\n"
           << "    r'': " << c.r_double_prime().full_simplify() << "\n"
           << "    T: " << c.T() << "\n"
           << "    N: " << c.N() << "\n"
           << "    κ: " << c.curvature().full_simplify() << "\n"
           << "    1/κ: " << c.inv_curvature().full_simplify() << "\n"
           << "    Center: " << c.osculating_center();
        return os;
    }

    std::string to_string() const { std::stringstream ss; ss << *this; return ss.str(); }

private:
    vectors::SymbolicVectorN r_;
    std::string     param_;

    mutable std::unique_ptr<vectors::SymbolicVectorN> rp_;
    mutable std::unique_ptr<vectors::SymbolicVectorN> rpp_;
    mutable std::unique_ptr<vectors::SymbolicVectorN> rppp_;
    mutable std::unique_ptr<cas::Expression>      curvature_;
    mutable std::unique_ptr<vectors::SymbolicVectorN> T_;
    mutable std::unique_ptr<vectors::SymbolicVectorN> N_;
    mutable std::unique_ptr<cas::Expression>      inv_curvature_;
    mutable std::unique_ptr<vectors::SymbolicVectorN> osc_center_;
};

} // namespace geometry
} // namespace math
} // namespace fizmo

#endif // FIZMO_FRENET_FRAME_HPP