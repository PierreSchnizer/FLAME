
#include <string>
#include <algorithm>
#include <any>

#include "flame/linear.h"
#include "flame/state/vector.h"
#include "flame/state/matrix.h"


#define sqr(x)  ((x)*(x))
#define cube(x) ((x)*(x)*(x))

// Phase-space units.
#if false
    // Use [m, rad, m, rad, rad, eV/u].
    #define MtoMM 1e0
#else
    // Use [mm, rad, mm, rad, rad, MeV/u].
    #define MtoMM 1e3
#endif

MatrixState::MatrixState(const Config& c)
    :StateBase(c)
    ,state(boost::numeric::ublas::identity_matrix<double>(maxsize))
{
    try{
        const std::vector<double>& I = c.get<std::vector<double> >("initial");
        if(I.size()>state.data().size())
            throw std::invalid_argument("Initial state size too big");
        std::copy(I.begin(), I.end(), state.data().begin());
    }catch(key_error&){
        // default to identity
    }catch(std::bad_any_cast&){
        throw std::invalid_argument("'initial' has wrong type (must be vector)");
    }
}

MatrixState::~MatrixState() {}

MatrixState::MatrixState(const MatrixState& o, clone_tag t)
    :StateBase(o, t)
    ,state(o.state)
{}

void MatrixState::assign(const StateBase& other)
{
    const MatrixState *O = dynamic_cast<const MatrixState*>(&other);
    if(!O)
        throw std::invalid_argument("Can't assign State: incompatible types");
    state = O->state;
    StateBase::assign(other);
}

void MatrixState::show(std::ostream& strm, int level) const
{
    strm<<"State: "<<state<<"\n";
}

bool MatrixState::getArray(unsigned idx, ArrayInfo& Info) {
    if(idx==0) {
        Info.name = "state";
        Info.ptr = &state(0,0);
        Info.type = ArrayInfo::Double;
        Info.ndim = 2;
        Info.dim[0] = state.size1();
        Info.dim[1] = state.size2();
        Info.stride[0] = sizeof(double)*state.size1();
        Info.stride[1] = sizeof(double);
        return true;
    }
    return StateBase::getArray(idx-1, Info);
}

VectorState::VectorState(const Config& c)
    :StateBase(c)
    ,state(maxsize, 0.0)
{
    try{
        const std::vector<double>& I = c.get<std::vector<double> >("initial");
        if(I.size()>state.size())
            throw std::invalid_argument("Initial state size too big");
        std::copy(I.begin(), I.end(), state.begin());
    }catch(key_error&){
    }catch(std::bad_any_cast&){
    }
}

VectorState::~VectorState() {}

VectorState::VectorState(const VectorState& o, clone_tag t)
    :StateBase(o, t)
    ,state(o.state)
{}

void VectorState::assign(const StateBase& other)
{
    const VectorState *O = dynamic_cast<const VectorState*>(&other);
    if(!O)
        throw std::invalid_argument("Can't assign State: incompatible types");
    state = O->state;
    StateBase::assign(other);
}

void VectorState::show(std::ostream& strm, int level) const
{
    strm<<"pos="<<pos<<" State: "<<state<<"\n";
}

bool VectorState::getArray(unsigned idx, ArrayInfo& Info) {
    if(idx==0) {
        Info.name = "state";
        Info.ptr = &state(0);
        Info.type = ArrayInfo::Double;
        Info.ndim = 1;
        Info.dim[0] = state.size();
        Info.stride[0] = sizeof(double);
        return true;
    }
    return StateBase::getArray(idx-1, Info);
}

namespace {

template<typename Base>
void Get2by2Matrix(const double L, const double K, const unsigned ind, typename Base::value_t &M)
{
    // Transport matrix for one plane for a Quadrupole.
    double sqrtK,
           psi,
           cs,
           sn;

    if (K > 0e0) {
        // Focusing.
        sqrtK = sqrt(K);
        psi = sqrtK*L;
        cs = ::cos(psi);
        sn = ::sin(psi);

        M(ind, ind) = M(ind+1, ind+1) = cs;
        if (sqrtK != 0e0)
            M(ind, ind+1) = sn/sqrtK;
        else
            M(ind, ind+1) = L;
        if (sqrtK != 0e0)
            M(ind+1, ind) = -sqrtK*sn;
        else
            M(ind+1, ind) = 0e0;
    } else {
        // Defocusing.
        sqrtK = sqrt(-K);
        psi = sqrtK*L;
        cs = ::cosh(psi);
        sn = ::sinh(psi);

        M(ind, ind) = M(ind+1, ind+1) = cs;
        if (sqrtK != 0e0)
            M(ind, ind+1) = sn/sqrtK;
        else
            M(ind, ind+1) = L;
        if (sqrtK != 0e0)
            M(ind+1, ind) = sqrtK*sn;
        else
            M(ind+1, ind) = 0e0;
    }
}

template<typename Base>
struct ElementSource : public Base
{
    typedef Base base_t;
    typedef typename base_t::state_t state_t;
    ElementSource(const Config& c)
        :base_t(c), istate(c)
    {}

    virtual void advance(StateBase& s) override final
    {
        state_t& ST = static_cast<state_t&>(s);
        // Replace state with our initial values
        ST.assign(istate);
    }

    virtual void show(std::ostream& strm, int level) const override final
    {
        ElementVoid::show(strm, level);
        strm<<"Initial: "<<istate.state<<"\n";
    }

    state_t istate;
    // note that 'transfer' is not used by this element type

    virtual ~ElementSource() {}

    virtual const char* type_name() const override final {return "source";}
};

template<typename Base>
struct ElementMark : public Base
{
    // Transport (identity) matrix for a Marker.
    typedef Base base_t;
    typedef typename base_t::state_t state_t;
    ElementMark(const Config& c)
        :base_t(c)
    {
//        double L = c.get<double>("L", 0e0);

        // Identity matrix.
    }
    virtual ~ElementMark() {}

    virtual const char* type_name() const override final {return "marker";}
};

template<typename Base>
struct ElementDrift : public Base
{
    // Transport matrix for a Drift.
    typedef Base base_t;
    typedef typename base_t::state_t state_t;
    ElementDrift(const Config& c)
        :base_t(c)
    {
        double L = this->length*MtoMM; // Convert from [m] to [mm].

        this->transfer(state_t::PS_X, state_t::PS_PX) = L;
        this->transfer(state_t::PS_Y, state_t::PS_PY) = L;
        // For total path length.
//        this->transfer(state_t::PS_S, state_t::PS_S)  = L;
    }
    virtual ~ElementDrift() {}

    virtual const char* type_name() const override final {return "drift";}
};

template<typename Base>
struct ElementSBend : public Base
{
    // Transport matrix for a Gradient Sector Bend (cylindrical coordinates).
    typedef Base base_t;
    typedef typename base_t::state_t state_t;
    ElementSBend(const Config& c)
        :base_t(c)
    {
        double L   = this->length*MtoMM,
               phi = c.get<double>("phi"),               // [rad].
               rho = L/phi,
               K   = c.get<double>("K", 0e0)/sqr(MtoMM), // [1/m^2].
               Kx  = K + 1e0/sqr(rho),
               Ky  = -K;

        // Horizontal plane.
        Get2by2Matrix<Base>(L, Kx, (unsigned)state_t::PS_X, this->transfer);
        // Vertical plane.
        Get2by2Matrix<Base>(L, Ky, (unsigned)state_t::PS_Y, this->transfer);
        // Longitudinal plane.
//        this->transfer(state_t::PS_S,  state_t::PS_S) = L;
    }
    virtual ~ElementSBend() {}

    virtual const char* type_name() const override final {return "sbend";}
};

template<typename Base>
struct ElementQuad : public Base
{
    // Transport matrix for a Quadrupole; K = B2/Brho.
    typedef Base base_t;
    typedef typename base_t::state_t state_t;
    ElementQuad(const Config& c)
        :base_t(c)
    {
        double L = this->length*MtoMM,
               //B2 = c.get<double>("B2"),
               K = c.get<double>("K", 0e0)/sqr(MtoMM);

        // Horizontal plane.
        Get2by2Matrix<Base>(L,  K, (unsigned)state_t::PS_X, this->transfer);
        // Vertical plane.
        Get2by2Matrix<Base>(L, -K, (unsigned)state_t::PS_Y, this->transfer);
        // Longitudinal plane.
        // For total path length.
//        this->transfer(state_t::PS_S, state_t::PS_S) = L;
    }
    virtual ~ElementQuad() {}

    virtual const char* type_name() const override final {return "quadrupole";}
};

template<typename Base>
struct ElementSolenoid : public Base
{
    // Transport (identity) matrix for a Solenoid; K = B0/(2 Brho).
    typedef Base base_t;
    typedef typename base_t::state_t state_t;
    ElementSolenoid(const Config& c)
        :base_t(c)
    {
        double L = this->length*MtoMM,      // Convert from [m] to [mm].
//               B = c.get<double>("B"),
               K = c.get<double>("K", 0e0)/MtoMM, // Convert from [m] to [mm].
               C = ::cos(K*L),
               S = ::sin(K*L);

        this->transfer(state_t::PS_X, state_t::PS_X)
                = this->transfer(state_t::PS_PX, state_t::PS_PX)
                = this->transfer(state_t::PS_Y, state_t::PS_Y)
                = this->transfer(state_t::PS_PY, state_t::PS_PY)
                = sqr(C);

        if (K != 0e0)
            this->transfer(state_t::PS_X, state_t::PS_PX) = S*C/K;
        else
            this->transfer(state_t::PS_X, state_t::PS_PX) = L;
        this->transfer(state_t::PS_X, state_t::PS_Y) = S*C;
        if (K != 0e0)
            this->transfer(state_t::PS_X, state_t::PS_PY) = sqr(S)/K;
        else
            this->transfer(state_t::PS_X, state_t::PS_PY) = 0e0;

        this->transfer(state_t::PS_PX, state_t::PS_X) = -K*S*C;
        this->transfer(state_t::PS_PX, state_t::PS_Y) = -K*sqr(S);
        this->transfer(state_t::PS_PX, state_t::PS_PY) = S*C;

        this->transfer(state_t::PS_Y, state_t::PS_X) = -S*C;
        if (K != 0e0)
            this->transfer(state_t::PS_Y, state_t::PS_PX) = -sqr(S)/K;
        else
            this->transfer(state_t::PS_Y, state_t::PS_PX) = 0e0;
        if (K != 0e0)
            this->transfer(state_t::PS_Y, state_t::PS_PY) = S*C/K;
        else
            this->transfer(state_t::PS_Y, state_t::PS_PY) = L;

        this->transfer(state_t::PS_PY, state_t::PS_X) = K*sqr(S);
        this->transfer(state_t::PS_PY, state_t::PS_PX) = -S*C;
        this->transfer(state_t::PS_PY, state_t::PS_Y) = -K*S*C;

        // Longitudinal plane.
        // For total path length.
//        this->transfer(state_t::PS_S, state_t::PS_S) = L;
    }
    virtual ~ElementSolenoid() {}

    virtual const char* type_name() const override final {return "solenoid";}
};

template<typename Base>
struct ElementGeneric : public Base
{
    typedef Base base_t;
    typedef typename base_t::state_t state_t;
    ElementGeneric(const Config& c)
        :base_t(c)
    {
        std::vector<double> I = c.get<std::vector<double> >("transfer");
        if(I.size()>this->transfer.data().size())
            throw std::invalid_argument("Initial transfer size too big");
        std::copy(I.begin(), I.end(), this->transfer.data().begin());
    }
    virtual ~ElementGeneric() {}

    virtual const char* type_name() const override final {return "generic";}
};

} // namespace

void registerLinear()
{
    Machine::registerState<VectorState>("Vector");
    Machine::registerState<MatrixState>("TransferMatrix");

    Machine::registerElement<ElementSource<LinearElementBase<VectorState>   > >("Vector",         "source");
    Machine::registerElement<ElementSource<LinearElementBase<MatrixState>   > >("TransferMatrix", "source");

    Machine::registerElement<ElementMark<LinearElementBase<VectorState>     > >("Vector",         "marker");
    Machine::registerElement<ElementMark<LinearElementBase<MatrixState>     > >("TransferMatrix", "marker");

    Machine::registerElement<ElementDrift<LinearElementBase<VectorState>    > >("Vector",         "drift");
    Machine::registerElement<ElementDrift<LinearElementBase<MatrixState>    > >("TransferMatrix", "drift");

    Machine::registerElement<ElementSBend<LinearElementBase<VectorState>    > >("Vector",         "sbend");
    Machine::registerElement<ElementSBend<LinearElementBase<MatrixState>    > >("TransferMatrix", "sbend");

    Machine::registerElement<ElementQuad<LinearElementBase<VectorState>     > >("Vector",         "quadrupole");
    Machine::registerElement<ElementQuad<LinearElementBase<MatrixState>     > >("TransferMatrix", "quadrupole");

    Machine::registerElement<ElementSolenoid<LinearElementBase<VectorState> > >("Vector",         "solenoid");
    Machine::registerElement<ElementSolenoid<LinearElementBase<MatrixState> > >("TransferMatrix", "solenoid");

    Machine::registerElement<ElementGeneric<LinearElementBase<VectorState>  > >("Vector",         "generic");
    Machine::registerElement<ElementGeneric<LinearElementBase<MatrixState>  > >("TransferMatrix", "generic");
}
