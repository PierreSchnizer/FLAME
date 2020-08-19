#ifndef FLAME_STATE_VECTOR_H
#define FLAME_STATE_VECTOR_H

#include <ostream>

#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/storage.hpp>

#include "../base.h"

/** @brief Simulation state which include only a vector
 */
struct VectorState : public StateBase
{
    enum {maxsize=6};
    enum param_t {
        PS_X, PS_PX, PS_Y, PS_PY, PS_S, PS_PS
    };

    VectorState(const Config& c);
    virtual ~VectorState();

    virtual void assign(const StateBase& other) override final;

    typedef boost::numeric::ublas::vector<double,
                    boost::numeric::ublas::bounded_array<double, maxsize>
    > value_t;

    virtual void show(std::ostream& strm, int level) const override final;

    value_t state;

    virtual bool getArray(unsigned idx, ArrayInfo& Info) override final;

    virtual VectorState* clone() const override final {
        return new VectorState(*this, clone_tag());
    }

protected:
    VectorState(const VectorState& o, clone_tag);
};

#endif // FLAME_STATE_VECTOR_H
