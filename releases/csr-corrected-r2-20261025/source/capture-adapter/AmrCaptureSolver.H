#ifndef LNJ_AMR_CAPTURE_SOLVER_H
#define LNJ_AMR_CAPTURE_SOLVER_H
// This header is safe at the OpenFOAM (not finiteVolume) library layer.
#include "scalarField.H"
#include <string>
namespace Foam {
class dictionary;
namespace amrCapture {
bool enabled();
bool equationActive();
const std::string& activeStage();
void text(const std::string&, const std::string&, const std::string&);
void value(const std::string&, const std::string&, scalar);
void scalars(const std::string&, const std::string&, const scalarUList&);
void firstAmul(const scalarField& actualInput, const scalarField& actualResult);
void setOperator(const void* actualOperator);
void observeAmul(const void* actualOperator, const scalarField&, const scalarField&);
void normFactor(scalar actualValue);
void solverControls(const dictionary&);
} }
#endif
