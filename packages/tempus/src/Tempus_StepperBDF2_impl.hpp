// @HEADER
// ****************************************************************************
//                Tempus: Copyright (2017) Sandia Corporation
//
// Distributed under BSD 3-clause license (See accompanying file Copyright.txt)
// ****************************************************************************
// @HEADER

#ifndef Tempus_StepperBDF2_impl_hpp
#define Tempus_StepperBDF2_impl_hpp

#include "Tempus_config.hpp"
#include "Tempus_StepperFactory.hpp"
#include "Tempus_WrapperModelEvaluatorBasic.hpp"
#include "Teuchos_VerboseObjectParameterListHelpers.hpp"
#include "NOX_Thyra.H"


namespace Tempus {

// Forward Declaration for recursive includes (this Stepper <--> StepperFactory)
template<class Scalar> class StepperFactory;


template<class Scalar>
StepperBDF2<Scalar>::StepperBDF2()
{
  this->setParameterList(Teuchos::null);
  this->modelWarning();
}


template<class Scalar>
StepperBDF2<Scalar>::StepperBDF2(
  const Teuchos::RCP<const Thyra::ModelEvaluator<Scalar> >& appModel,
  Teuchos::RCP<Teuchos::ParameterList> pList)
{
  this->setParameterList(pList);

  if (appModel == Teuchos::null) {
    this->modelWarning();
  }
  else {
    this->setModel(appModel);
    this->initialize();
  }
}


/** \brief Set the startup stepper to a pre-defined stepper in the ParameterList
 *
 *  The startup stepper is set to stepperName sublist in the Stepper's
 *  ParameterList.  The stepperName sublist should already be defined
 *  in the Stepper's ParameterList.  Otherwise it will fail.
 */
template<class Scalar>
void StepperBDF2<Scalar>::setStartUpStepper(std::string startupStepperName)
{
  using Teuchos::RCP;
  using Teuchos::ParameterList;

  RCP<ParameterList> startupStepperPL =
    Teuchos::sublist(this->stepperPL_, startupStepperName, true);
  this->stepperPL_->set("Start Up Stepper Name", startupStepperName);
  RCP<StepperFactory<Scalar> > sf = Teuchos::rcp(new StepperFactory<Scalar>());
  startUpStepper_ =
    sf->createStepper(startupStepperPL, this->wrapperModel_->getAppModel());

  this->isInitialized_ = false;
}


/** \brief Set the start up stepper to the supplied Parameter sublist.
 *
 *  This adds a new start up stepper Parameter sublist to the Stepper's
 *  ParameterList.  If the start up stepper sublist is null, it tests if
 *  the stepper sublist is set in the Stepper's ParameterList.
 */
template<class Scalar>
void StepperBDF2<Scalar>::setStartUpStepper(
  Teuchos::RCP<Teuchos::ParameterList> startupStepperPL)
{
  using Teuchos::RCP;
  using Teuchos::ParameterList;

  Teuchos::RCP<Teuchos::ParameterList> stepperPL = this->stepperPL_;
  std::string startupStepperName =
    stepperPL->get<std::string>("Start Up Stepper Name","None");
  if (is_null(startupStepperPL)) {
    // Create startUpStepper, otherwise keep current startUpStepper.
    if (startUpStepper_ == Teuchos::null) {
      if (startupStepperName != "None") {
        // Construct from ParameterList
        startupStepperPL =
          Teuchos::sublist(this->stepperPL_, startupStepperName, true);
        RCP<StepperFactory<Scalar> > sf =
          Teuchos::rcp(new StepperFactory<Scalar>());
        startUpStepper_ =
          sf->createStepper(startupStepperPL,this->wrapperModel_->getAppModel());
      } else {
        // Construct default start-up Stepper
        RCP<StepperFactory<Scalar> > sf =
          Teuchos::rcp(new StepperFactory<Scalar>());
        startUpStepper_ =
          sf->createStepper("IRK 1 Stage Theta Method",
                            this->wrapperModel_->getAppModel());

        startupStepperName = startUpStepper_->description();
        startupStepperPL = startUpStepper_->getNonconstParameterList();
        this->stepperPL_->set("Start Up Stepper Name", startupStepperName);
        this->stepperPL_->set(startupStepperName, *startupStepperPL);  // Add sublist
      }
    }
  } else {
    TEUCHOS_TEST_FOR_EXCEPTION( startupStepperName == startupStepperPL->name(),
      std::logic_error,
         "Error - Trying to add a startup stepper that is already in "
      << "ParameterList!\n"
      << "  Stepper Type = "<< stepperPL->get<std::string>("Stepper Type")
      << "\n" << "  Start Up Stepper Name  = "<<startupStepperName<<"\n");
    startupStepperName = startupStepperPL->name();
    this->stepperPL_->set("Start Up Stepper Name", startupStepperName);
    this->stepperPL_->set(startupStepperName, *startupStepperPL);     // Add sublist
    RCP<StepperFactory<Scalar> > sf =
      Teuchos::rcp(new StepperFactory<Scalar>());
    startUpStepper_ =
      sf->createStepper(startupStepperPL, this->wrapperModel_->getAppModel());
  }
  this->isInitialized_ = false;
}


template<class Scalar>
void StepperBDF2<Scalar>::setObserver(
  Teuchos::RCP<StepperObserver<Scalar> > obs)
{
  if (obs == Teuchos::null) {
    // Create default observer, otherwise keep current observer.
    if (this->stepperObserver_ == Teuchos::null) {
      stepperBDF2Observer_ =
        Teuchos::rcp(new StepperBDF2Observer<Scalar>());
      this->stepperObserver_ =
        Teuchos::rcp_dynamic_cast<StepperObserver<Scalar> >
          (stepperBDF2Observer_);
     }
  } else {
    this->stepperObserver_ = obs;
    stepperBDF2Observer_ =
      Teuchos::rcp_dynamic_cast<StepperBDF2Observer<Scalar> >(this->stepperObserver_);
  }
  this->isInitialized_ = false;
}


template<class Scalar>
void StepperBDF2<Scalar>::initialize()
{
  TEUCHOS_TEST_FOR_EXCEPTION( this->wrapperModel_ == Teuchos::null,
    std::logic_error,
    "Error - Need to set the model, setModel(), before calling "
    "StepperBDF2::initialize()\n");

  this->setParameterList(this->stepperPL_);
  this->setSolver();
  this->setStartUpStepper();
  this->startUpStepper_->initialize();
  this->setObserver();
  order_ = Scalar(2.0);

  this->isInitialized_ = true;   // Only place where it should be set to true.
}

template<class Scalar>
bool StepperBDF2<Scalar>::isInitialized()
{
  return (this->isInitialized_ && startUpStepper_->isInitialized());
}

template<class Scalar>
void StepperBDF2<Scalar>::setInitialConditions(
  const Teuchos::RCP<SolutionHistory<Scalar> >& solutionHistory)
{
  using Teuchos::RCP;

  RCP<SolutionState<Scalar> > initialState = solutionHistory->getCurrentState();

  // Check if we need Stepper storage for xDot
  if (initialState->getXDot() == Teuchos::null)
    this->setStepperXDot(initialState->getX()->clone_v());

  StepperImplicit<Scalar>::setInitialConditions(solutionHistory);

  if (this->getUseFSAL()) {
    RCP<Teuchos::FancyOStream> out = this->getOStream();
    Teuchos::OSTab ostab(out,1,"StepperBDF2::setInitialConditions()");
    *out << "\nWarning -- The First-Step-As-Last (FSAL) principle is not "
         << "needed with Backward Euler.  The default is to set useFSAL=false, "
         << "however useFSAL=true will also work but have no affect "
         << "(i.e., no-op).\n" << std::endl;
  }
  this->isInitialized_ = false;
}


template<class Scalar>
void StepperBDF2<Scalar>::takeStep(
  const Teuchos::RCP<SolutionHistory<Scalar> >& solutionHistory)
{
  TEUCHOS_TEST_FOR_EXCEPTION( !this->isInitialized(), std::logic_error,
    "Error - " << this->description() << " is not initialized!");

  using Teuchos::RCP;

  TEMPUS_FUNC_TIME_MONITOR("Tempus::StepperBDF2::takeStep()");
  {
    int numStates = solutionHistory->getNumStates();

    RCP<Thyra::VectorBase<Scalar> > xOld;
    RCP<Thyra::VectorBase<Scalar> > xOldOld;

    // If there are less than 3 states (e.g., first time step), call
    // startup stepper and return.
    if (numStates < 3) {
      computeStartUp(solutionHistory);
      return;
    }
    TEUCHOS_TEST_FOR_EXCEPTION( (numStates < 3), std::logic_error,
      "Error in Tempus::StepperBDF2::takeStep(): numStates after \n"
        << "startup stepper must be at least 3, whereas numStates = "
        << numStates <<"!\n" << "If running with Storage Type = Static, "
        << "make sure Storage Limit > 2.\n");

    //IKT, FIXME: add error checking regarding states being consecutive and
    //whether interpolated states are OK to use.

    this->stepperObserver_->observeBeginTakeStep(solutionHistory, *this);

    RCP<SolutionState<Scalar> > workingState=solutionHistory->getWorkingState();
    RCP<SolutionState<Scalar> > currentState=solutionHistory->getCurrentState();

    RCP<Thyra::VectorBase<Scalar> > x    = workingState->getX();
    RCP<Thyra::VectorBase<Scalar> > xDot = this->getStepperXDot(workingState);

    //get time, dt and dtOld
    const Scalar time  = workingState->getTime();
    const Scalar dt    = workingState->getTimeStep();
    const Scalar dtOld = currentState->getTimeStep();

    xOld    = solutionHistory->getStateTimeIndexNM1()->getX();
    xOldOld = solutionHistory->getStateTimeIndexNM2()->getX();
    order_ = Scalar(2.0);

    // Setup TimeDerivative
    Teuchos::RCP<TimeDerivative<Scalar> > timeDer =
      Teuchos::rcp(new StepperBDF2TimeDerivative<Scalar>(dt, dtOld, xOld, xOldOld));

    const Scalar alpha = getAlpha(dt, dtOld);
    const Scalar beta  = getBeta (dt);

    Teuchos::RCP<ImplicitODEParameters<Scalar> > p =
      Teuchos::rcp(new ImplicitODEParameters<Scalar>(timeDer,dt,alpha,beta,
                                                     SOLVE_FOR_X));

    const Thyra::SolveStatus<Scalar> sStatus =
      this->solveImplicitODE(x, xDot, time, p);

    if (!Teuchos::is_null(stepperBDF2Observer_))
      stepperBDF2Observer_->observeAfterSolve(solutionHistory, *this);

    if (workingState->getXDot() != Teuchos::null)
      timeDer->compute(x, xDot);

    workingState->setSolutionStatus(sStatus);  // Converged --> pass.
    workingState->setOrder(getOrder());
    this->stepperObserver_->observeEndTakeStep(solutionHistory, *this);
  }
  return;
}

template<class Scalar>
void StepperBDF2<Scalar>::computeStartUp(
      const Teuchos::RCP<SolutionHistory<Scalar> >& solutionHistory)
{
  Teuchos::RCP<Teuchos::FancyOStream> out = this->getOStream();
  Teuchos::OSTab ostab(out,1,"StepperBDF2::computeStartUp()");
  *out << "Warning -- Taking a startup step for BDF2 using '"
       << startUpStepper_->description()<<"'!" << std::endl;

  //Take one step using startUpStepper_
  startUpStepper_->takeStep(solutionHistory);

  order_ = startUpStepper_->getOrder();
}

/** \brief Provide a StepperState to the SolutionState.
 *  This Stepper does not have any special state data,
 *  so just provide the base class StepperState with the
 *  Stepper description.  This can be checked to ensure
 *  that the input StepperState can be used by this Stepper.
 */
template<class Scalar>
Teuchos::RCP<Tempus::StepperState<Scalar> >
StepperBDF2<Scalar>::
getDefaultStepperState()
{
  Teuchos::RCP<Tempus::StepperState<Scalar> > stepperState =
    rcp(new StepperState<Scalar>(description()));
  return stepperState;
}


template<class Scalar>
std::string StepperBDF2<Scalar>::description() const
{
  std::string name = "BDF2";
  return(name);
}


template<class Scalar>
void StepperBDF2<Scalar>::describe(
   Teuchos::FancyOStream               &out,
   const Teuchos::EVerbosityLevel      /* verbLevel */) const
{
  out << description() << "::describe:" << std::endl
      << "wrapperModel_ = " << this->wrapperModel_->description() << std::endl;
}


template <class Scalar>
void StepperBDF2<Scalar>::setParameterList(
  Teuchos::RCP<Teuchos::ParameterList> const& pList)
{
  Teuchos::RCP<Teuchos::ParameterList> stepperPL = this->stepperPL_;
  if (pList == Teuchos::null) {
    // Create default parameters if null, otherwise keep current parameters.
    if (stepperPL == Teuchos::null) stepperPL = this->getDefaultParameters();
  } else {
    stepperPL = pList;
  }
  if (!(stepperPL->isParameter("Solver Name"))) {
    stepperPL->set<std::string>("Solver Name", "Default Solver");
    Teuchos::RCP<Teuchos::ParameterList> solverPL =
      this->defaultSolverParameters();
    stepperPL->set("Default Solver", *solverPL);
  }
  // Can not validate because of optional Parameters (e.g., Solver Name).
  //stepperPL->validateParametersAndSetDefaults(*this->getValidParameters());

  std::string stepperType = stepperPL->get<std::string>("Stepper Type");
  TEUCHOS_TEST_FOR_EXCEPTION( stepperType != "BDF2", std::logic_error,
       "Error - Stepper Type is not 'BDF2'!\n"
    << "  Stepper Type = "<<stepperPL->get<std::string>("Stepper Type")<<"\n");

  this->stepperPL_ = stepperPL;
  this->isInitialized_ = false;
}


template<class Scalar>
Teuchos::RCP<const Teuchos::ParameterList>
StepperBDF2<Scalar>::getValidParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> pl = Teuchos::parameterList();
  pl->setName("Default Stepper - " + this->description());
  pl->set<std::string>("Stepper Type", this->description());
  this->getValidParametersBasic(pl);
  pl->set<bool>("Initial Condition Consistency Check", false);
  pl->set<bool>("Zero Initial Guess", false);
  pl->set<std::string>("Solver Name", "",
    "Name of ParameterList containing the solver specifications.");

  return pl;
}


template<class Scalar>
Teuchos::RCP<Teuchos::ParameterList>
StepperBDF2<Scalar>::getDefaultParameters() const
{
  using Teuchos::RCP;
  using Teuchos::ParameterList;
  using Teuchos::rcp_const_cast;

  RCP<ParameterList> pl =
    rcp_const_cast<ParameterList>(this->getValidParameters());

  pl->set<std::string>("Solver Name", "Default Solver");
  RCP<ParameterList> solverPL = this->defaultSolverParameters();
  pl->set("Default Solver", *solverPL);

  return pl;
}


template <class Scalar>
Teuchos::RCP<Teuchos::ParameterList>
StepperBDF2<Scalar>::getNonconstParameterList()
{
  return(this->stepperPL_);
}


template <class Scalar>
Teuchos::RCP<Teuchos::ParameterList>
StepperBDF2<Scalar>::unsetParameterList()
{
  Teuchos::RCP<Teuchos::ParameterList> temp_plist = this->stepperPL_;
  this->stepperPL_ = Teuchos::null;
  return(temp_plist);
}


} // namespace Tempus
#endif // Tempus_StepperBDF2_impl_hpp
