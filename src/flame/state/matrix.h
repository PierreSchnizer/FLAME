#ifndef FLAME_STATE_MATRIX_HPP
#define FLAME_STATE_MATRIX_HPP

#include <ostream>

#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/storage.hpp>

#include "../base.h"

/** @brief Simulation state which include only a matrix
 */
struct MatrixState : public StateBase
{
    enum {maxsize=6};
    enum param_t {
        PS_X, PS_PX, PS_Y, PS_PY, PS_S, PS_PS
    };

    MatrixState(const Config& c);
    virtual ~MatrixState();

    void assign(const StateBase& other);

    typedef boost::numeric::ublas::matrix<double,
                    boost::numeric::ublas::row_major,
                    boost::numeric::ublas::bounded_array<double, maxsize*maxsize>
    > value_t;

    virtual void show(std::ostream& strm, int level) const override final;

    value_t state;

    virtual bool getArray(unsigned idx, ArrayInfo& Info) override final;

    virtual MatrixState* clone() const override final {
        return new MatrixState(*this, clone_tag());
    }

protected:
    MatrixState(const MatrixState& o, clone_tag);
};

#endif // FLAME_STATE_MATRIX_HPP
