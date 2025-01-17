#include "M2AdmittanceStates.h"
#include "M2Admittance.h"
#include <stdlib.h>
#include <bits/stdc++.h>
//#include <complex>

#define OWNER ((M2Admittance *)owner)


int EveryN=1;//Process every N samples: reduce frequency

double timeval_to_sec(struct timespec *ts)
{
    return (double)(ts->tv_sec + ts->tv_nsec / 1000000000.0);
}

VM2 myVE(VM2 X, VM2 dX, VM2 Fm, Eigen::Matrix2d B, Eigen::Matrix2d M, double dt)
{
    Eigen::Matrix2d realM;
    Eigen::Matrix2d realB;

    if(X(0)<0.30)
    {
     realB(0,0) = 15.0;
     realB(1,1) = 15.0;
     realM(0,0) = 10.0;
     realM(1,1) = 10.0;
    }
    else
    {
     realB = B;
     realM = M;
     /*M(0,0) = 2.0;
     M(1,1) = 2.0;*/
    }

    Eigen::Matrix2d Operator;
     Operator(0,0) = 1/(realM(0,0) + realB(0,0)*dt);
     Operator(1,1) = 1/(realM(1,1) + realB(1,1)*dt);

    //return ;//
    return Operator*(Fm*dt + realM*dX);
}

//VM2 myPO_obs(VM2 Vm, VM2 Fm, double dt)
//{
    //return ;//
    //Eigen::Matrix2d Em;
    //Em = Matrix2d::Zero(2,2);
    //VM2 Em;
//Em = Em + dt*Vm.cwiseProduct(Fm);
   // return Em;
//}

VM2 myPOobs(VM2 Vm, VM2 Fm, double dt)
{
    //return ;//
    //Eigen::Matrix2d Em;
    //Em = Matrix2d::Zero(2,2);
    VM2 Em;
    Em = Vm.cwiseProduct(Fm);
    return Em;
}

VM2 myPOref(VM2 Ve, VM2 Fm, double dt)
{
   //VM2 Vd =(Fm*dt + M*dX)*Operator;
   //return;
   //Eigen::Matrix2d Eref; //=  Matrix2d::Zero(2,2);
  VM2 Eref;
  Eref = Eref + dt*Ve.cwiseProduct(Fm);
  return Eref;
}

VM2 myPOerror(VM2 Vm, VM2 Ve, VM2 Fm, double dt)
{
   VM2 Pref;
   VM2 Pobs;
   VM2 Pdev;

   Pobs = Vm.cwiseProduct(Fm);
   Pref = Ve.cwiseProduct(Fm);
   Pdev = Pref - Pobs;
   return Pdev;
}

VM2 impedance(Eigen::Matrix2d K, Eigen::Matrix2d D, VM2 X0, VM2 X, VM2 dX, VM2 dXd=VM2::Zero()) {
    return K*(X0-X) + D*(dXd-dX); //K is equivlent to the P gain in position control, D is equivlent to the D gain in position control the reference point is the messured velocity
}

//minJerk(X0, Xf, T, t, &X, &dX)
double JerkIt(VM2 X0, VM2 Xf, double T, double t, VM2 &Xd, VM2 &dXd) {
    t = std::max(std::min(t, T), .0); //Bound time
    double tn=std::max(std::min(t/T, 1.0), .0);//Normalised time bounded 0-1
    double tn3=pow(tn,3.);
    double tn4=tn*tn3;
    double tn5=tn*tn4;
    Xd = X0 + ( (X0-Xf) * (15.*tn4-6.*tn5-10.*tn3) );
    dXd = (X0-Xf) * (4.*15.*tn4-5.*6.*tn5-10.*3*tn3)/t;
    return tn;
}


void M2Calib::entryCode(void) {
    calibDone=false;
    for(unsigned int i=0; i<2; i++) {
        stop_reached_time[i] = .0;
        at_stop[i] = false;
    }
    robot->decalibrate();
    robot->initTorqueControl();
   // robot -> printStatus();
    robot->printJointStatus();
    std::cout << "Calibrating (keep clear)..." << std::flush;
}

//Move slowly on each joint until max force detected
void M2Calib::duringCode(void) {
    VM2 tau(0, 0);

    //Apply constant torque (with damping) unless stop has been detected for more than 0.5s
    VM2 vel=robot->getVelocity();
    double b = 3;
    for(int i=0; i<vel.size(); i++) {
        tau(i) = -std::min(std::max(20 - b * vel(i), .0), 20.);
        if(stop_reached_time(i)>1) {
            at_stop[i]=true;
        }
        if(abs(vel(i))<0.005) {
            stop_reached_time(i) += dt;
        }
    }

    //Switch to gravity control when done
    if(robot->isCalibrated()) {
        robot->setEndEffForceWithCompensation(VM2::Zero(), false);
        calibDone=true; //Trigger event
    }
    else {
        //If all joints are calibrated
        if(at_stop[0] && at_stop[1]) {
            robot->applyCalibration();
            std::cout << "OK." << std::endl;
        }
        else {
            robot->setJointTorque(tau);
            if(iterations%100==1) {
                std::cout << "." << std::flush;
            }
        }
    }
}

void M2Calib::exitCode(void) {
    robot->setEndEffForceWithCompensation(VM2::Zero());
}

void M2Transparent::entryCode(void) {
    robot->initTorqueControl();
    ForceP(0,0) = 1.3;
    ForceP(1,1) = 1.4;
}

void M2Transparent::duringCode(void) {

    //Smooth transition in case a mass is set at startup
    //double settling_time = 3.0;
    //double t=elapsedTime>settling_time?1.0:elapsedTime/settling_time;

    //Apply corresponding force
    VM2 f_m = robot->getInteractionForceRef();
    robot->setEndEffForce(ForceP*f_m);

    if(iterations%100==1) {
        robot->printStatus();
    }

    //Read keyboard inputs to change gain
    if(robot->keyboard->getS()) {
        ForceP(1,1)-=0.1;
        std::cout << ForceP <<std::endl;
        std::cout << ForceP*f_m <<std::endl;
    }
    if(robot->keyboard->getW()) {
        ForceP(1,1)+=0.1;
        std::cout << ForceP(1,1) <<std::endl;
        std::cout << ForceP*f_m <<std::endl;
    }
}

void M2Transparent::exitCode(void) {
    robot->setEndEffForce(VM2::Zero());
}

void M2MinJerkPosition::entryCode(void) {
    //Setup velocity control for position over velocity loop
    robot->initVelocityControl();
    robot->setJointVelocity(VM2::Zero());
    goToNextVel=false;
    trialDone=false;

    startTime=elapsedTime;
    Xi = robot->getEndEffPosition();
    VM2 destination;
    destination(0)=0.2;
    destination(1)=0.2;
    Xf = destination;
    T=3; //Trajectory Time
    k_i=1.;
}

void M2MinJerkPosition::duringCode(void) {
    VM2 Xd, dXd;
    //Compute current desired interpolated point
    double status=JerkIt(Xi, Xf, T, elapsedTime-startTime, Xd, dXd);
    //Apply position control
    if(robot->isEnabled())
    {
        robot->setEndEffVelocity(dXd+k_i*(Xd-robot->getEndEffPosition()));
    }
}

void M2MinJerkPosition::exitCode(void) {
    robot->setJointVelocity(VM2::Zero());
}

// M2Admittance1: the original strategy

void M2Admittance1::entryCode(void) {
    //Setup velocity control for position over velocity loop
    robot->initVelocityControl();
    robot->setJointVelocity(VM2::Zero());
    M(0,0)=M(1,1)= 2.0;
    B(0,0)=B(1,1)= 1.0;

    Eobs(0) = 0.0;
    Eobs(1) = 0.0;

    ObsvT = 1000;
    i = 0;

    El(0) = El(1) = -2;
    Eu(0) = Eu(1) = 2;

    X(VM2::Zero());
    dX(VM2::Zero());
    Fm(VM2::Zero());
    Vd(VM2::Zero());
    Verror(VM2::Zero());
    Power(VM2::Zero());

    stateLogger.initLogger("M2Admittance1State", "logs/M2Admittance1State.csv", LogFormat::CSV, true);
    stateLogger.add(elapsedTime, "Time (s)");
    stateLogger.add(robot->getEndEffPositionRef(), "Position");
    stateLogger.add(robot->getEndEffVelocityRef(), "Velocity");
    stateLogger.add(robot->getInteractionForceRef(), "Force");
    stateLogger.add(Eobs,"Observed_Energy");

    stateLogger.add(robot->getEndEffPositionRef(), "Position");
    stateLogger.add(robot->getEndEffVelocityRef(), "Velocity");
    stateLogger.add(robot->getInteractionForceRef(), "Force");
    stateLogger.add(Verror,"velocity_error");
    stateLogger.add(Power,"Power_PO");
    //
    stateLogger.add((M(0,0),M(1,1)), "Virutal_mass");
    //stateLogger.add((Md(0,0),Md(1,1)),"Desired_mass");
    stateLogger.add((B(0,0),B(1,1)),"Virutal_damping");
    //stateLogger.add((Bd(0,0),Bd(1,1)),"Desired_damping");
    //
   //stateLogger.add(Eobs,"Observed_Energy");
    stateLogger.startLogger();
}

void M2Admittance1::duringCode(void) {
    //VM2 X, dX, Fm, Vd;
    //get robot position and velocity and force mesaure

    X = robot->getEndEffPosition();
    dX = robot->getEndEffVelocity();
    Fm = robot->getInteractionForceRef();

    //Change Mass
    if(robot->keyboard->getQ()) {
        M(1,1)-=0.1;
        std::cout <<  M <<std::endl;
    }
    if(robot->keyboard->getA()) {
        M(1,1)+=0.1;
        std::cout <<  M <<std::endl;
    }

    //calculate VE velocity (X,dx)
     //myVE(X, dX, Fm, M, Operator, dt);
    Verror = Vd - dX;
    Power = myPOerror(dX,Vd,Fm,dt)*dt;

    Vd = myVE(X, dX, Fm, B, M, dt);

    if (i <= ObsvT)
    {
    //Eobs = Eobs + myPO_obs(dX, Fm, dt);
     Eobs = Eobs + myPOobs(dX, Fm, dt)*dt;
     i += 1;
     //if (Eobs(0) >= Eu(0)){Eobs(0) = Eu(0);}
     //if (Eobs(1) >= Eu(1)){Eobs(1) = Eu(1);}
     //if (Eobs(0) <= El(0)){Eobs(0) = El(0);}
     //if (Eobs(1) <= El(1)){Eobs(1) = El(1);}
    }
    if (i > ObsvT)
    {
     i = 0;
     Eobs(VM2::Zero());
    }

    //apply velocity
    //robot->setEndEffVelocity();
    robot->setEndEffVelocity(Vd);
    stateLogger.recordLogData();
}

void M2Admittance1::exitCode(void) {
    robot->setJointVelocity(VM2::Zero());
}

//Own strategy, the admittance controller algorithm is work as
void M2Admittance2::entryCode(void) {
    //Setup velocity control for position over velocity loop
    robot->initVelocityControl();
    robot->setJointVelocity(VM2::Zero());

    M(0,0)=M(1,1)=Md(0,0)=Md(1,1)=0.5;
    B(0,0)=B(1,1)=Bd(0,0)=Bd(1,1)=1.0;

    Eobs(0) = 0.0;
    Eobs(1) = 0.0;

    // X(VM2::Zero());
    // dX(VM2::Zero());
    // Fm(VM2::Zero());
    // Vd(VM2::Zero());

    POx = 0.0;
    POy = 0.0;

    gainx = 1.0;
    gainy = 1.0;

    alphax = 0.2;
    alphay = 0.2;

    ObsvT = 1000;
    i = 0;

    X(VM2::Zero());
    dX(VM2::Zero());
    Fm(VM2::Zero());
    Vd(VM2::Zero());

    Verror(VM2::Zero());

    El(0) = El(1) = -2;
    Eu(0) = Eu(1) = 2;
    //Obsv_Tx = 100;
    //Obsv_Ty = 100;

   stateLogger.initLogger("M2Admittance2State", "logs/M2Admittance2State.csv", LogFormat::CSV, true);
   stateLogger.add(elapsedTime, "Time (s)");
   stateLogger.add(robot->getEndEffPositionRef(), "Position");
   stateLogger.add(robot->getEndEffVelocityRef(), "Velocity");
   stateLogger.add(robot->getInteractionForceRef(), "Force");
   //
   stateLogger.add(Verror,"velocity_error");
   //
   stateLogger.add((M(0,0),M(1,1)), "Virutal_mass");
   stateLogger.add((Md(0,0),Md(1,1)),"Desired_mass");
   stateLogger.add((B(0,0),B(1,1)),"Virutal_damping");
   stateLogger.add((Bd(0,0),Bd(1,1)),"Desired_damping");
   //
   stateLogger.add(Eobs,"Observed_Energy");
   stateLogger.startLogger();
}

void M2Admittance2::duringCode(void) {
    //Eigen::Matrix2d B;
    //Eigen::Matrix2d M;
    //Eigen::Matrix2d Operator;
    //double PO_x, PO_y, gain_x =1.0, gain_y=1.0, alpha_x =0.2, alpha_y=0.2;
    //VM2 X, dX, Fm, Vd;
    //get robot position and velocity and force mesaure

     X = robot->getEndEffPosition();
     dX = robot->getEndEffVelocity();
     Fm = robot->getInteractionForceRef();

     //calculate VE velocity (X,dx)
     //myVE(X, dX, Fm, M, Operator, dt);
     if(robot->keyboard->getQ())
     {
        Md(1,1)-= 0.1;
        std::cout <<  Md(1,1) <<std::endl;
     }

    if(robot->keyboard->getA())
    {
        Md(1,1)+= 0.1;
        std::cout <<  Md(1,1) <<std::endl;
    }

    M(1,1) = Md(1,1);
    Vd = myVE(X, dX, Fm, B, M, dt);

    Verror = Vd - dX;

    if (i <= ObsvT)
    {
    //Eobs = Eobs + myPO_obs(dX, Fm, dt);
     Eobs = Eobs + myPOobs(dX, Fm, dt)*dt;
     i += 1;
     if (Eobs(0) >= Eu(0)){Eobs(0) = Eu(0);}
     if (Eobs(0) <= El(0)){Eobs(0) = El(0);}
     else{Eobs(0)=Eobs(0);}
     if (Eobs(1) >= Eu(1)){Eobs(1) = Eu(1);}
     if (Eobs(1) <= El(1)){Eobs(1) = El(1);}
     else{Eobs(0)=Eobs(0);}
    }
    if (i > ObsvT)
    {
     i = 0;
     Eobs(VM2::Zero());
     std::cout << "A2 PO is reseted" << std::endl;
    }

     if(iterations%100==1)
     {
        robot->printStatus();
        std::cout << Eobs.transpose() << std::endl;
     }

    //if(robot->keyboard->getQ()) {
       //change stuff
    //}
    // if(robot->keyboard->getA()) {
       //change stuff
    //}

    POx = Eobs(0);
    POy = Eobs(1);

    if (POx <= El(0)){M(0,0) = 5;}
    if (POx >= Eu(0)){M(0,0) = Md(0,0);}
    else{M(0,0) = M(0,0);}
    //Vd(0) = gainx * Vd(0);

    if (POy <= El(1)){M(1,1) = 5;}
    if(POy >= Eu(1)){M(1,1) = Md(1,1);}
    else{M(1,1) = M(1,1);}

    M(0,0) = max(M(0,0),0.01);
    M(1,1) = max(M(1,1),0.01);
    //Vd(1) = gainy * Vd(1);
    Vd = myVE(X, dX, Fm, B, M, dt);
    //apply velocity
    //robot->setEndEffVelocity();
    robot->setEndEffVelocity(Vd);
//
    stateLogger.recordLogData();
}

void M2Admittance2::exitCode(void) {
    robot->setJointVelocity(VM2::Zero());
}

//Classic PO
void M2Admittance3::entryCode(void) {
    //Setup velocity control for position over velocity loop
    //robot->initVelocityControl();
    //robot->setJointVelocity(VM2::Zero());
    //M(0,0)=M(1,1)=10.0;
    //B(0,0)=B(1,1)=10.0;

    robot->initVelocityControl();
    robot->setJointVelocity(VM2::Zero());

    M(0,0)=M(1,1)=Md(0,0)=Md(1,1)= 0.5;
    B(0,0)=B(1,1)=Bd(0,0)=Bd(1,1)= 2.0;

    Eobs(0) = 0.0;
    Eobs(1) = 0.0;

    X(VM2::Zero());
    dX(VM2::Zero());
    Fm(VM2::Zero());
    Vd(VM2::Zero());
    Power(VM2::Zero());

    PO_x = 0.0;
    PO_y = 0.0;

    gain_x = 1.0;
    gain_y = 1.0;

    alpha_x = 5;
    alpha_y = 5;

    Obsv_T = 100;
    i = 1;

    Del(0) = Del(1) = 0.8;
    El(0) = El(1) = -0.5;
    Eu(0) = Eu(1) = 0.5;

    //Obsv_Tx = 100;
    //Obsv_Ty = 100;
    stateLogger.initLogger("M2Admittance3State", "logs/M2Admittance3State.csv", LogFormat::CSV, true);
    stateLogger.add(elapsedTime, "Time (s)");
    stateLogger.add(robot->getEndEffPositionRef(), "Position");
    stateLogger.add(robot->getEndEffVelocityRef(), "Velocity");
    stateLogger.add(robot->getInteractionForceRef(), "Force");
    stateLogger.add((M(0,0),M(1,1)), "Virutal_mass");
    stateLogger.add((Md(0,0),Md(1,1)),"Desired_mass");
    stateLogger.add((B(0,0),B(1,1)),"Virutal_damping");
    stateLogger.add((Bd(0,0),Bd(1,1)),"Desired_damping");
    stateLogger.add(Eobs,"Observed_Energy");
    //
    stateLogger.startLogger();
}

void M2Admittance3::duringCode(void) {
    VM2 X, dX, Fm, Vd;
    // Eigen::Matrix2d B;
    // Eigen::Matrix2d M;
    // Eigen::Matrix2d Operator;
    // get robot position and velocity and force mesaure

    // X = robot->getEndEffPosition();
    // dX = robot->getEndEffVelocity();
    // Fm = robot->getInteractionForceRef();

    // calculate VE velocity (X,dx)
    // myVE(X, dX, Fm, M, Operator, dt);
    // Vd = myVE(X, dX, Fm, B, M, dt);

    // apply velocity
    // robot->setEndEffVelocity();
    // robot->setEndEffVelocity(Vd);

     X = robot->getEndEffPosition();
     dX = robot->getEndEffVelocity();
     Fm = robot->getInteractionForceRef();

     //calculate VE velocity (X,dx)
     //myVE(X, dX, Fm, M, Operator, dt);
     //if(robot->keyboard->getQ()) {
     //   M(1,1)-=0.1;
     //   std::cout <<  M(1,1) <<std::endl;
     //}
     //if(robot->keyboard->getA()) {
     //    M(1,1)+=0.1;
     //   std::cout <<  M(1,1) <<std::endl;
     // }

    Vd = myVE(X, dX, Fm, B, M, dt);


     if (i <= Obsv_T)
    {
    //Eobs = Eobs + myPOobs(dX, Fm, dt);
     Eobs = Eobs + myPOobs(dX, Fm, dt)*dt;
     i += 1;
     if (Eobs(0) >= Eu(0)){Eobs(0) = Eu(0);}
     if (Eobs(0) <= El(0)){Eobs(0) = El(0);}
     else{Eobs(0)=Eobs(0);}
     if (Eobs(1) >= Eu(1)){Eobs(1) = Eu(1);}
     if (Eobs(1) <= El(1)){Eobs(1) = El(1);}
     else{Eobs(0)=Eobs(0);}
    }
    if (i > Obsv_T)
    {
     i = 0;
     Eobs(VM2::Zero());
     std::cout << "A2 PO is reseted" << std::endl;
    }
  //myPOobs
     if(iterations%100==1)
     {
        robot->printStatus();
        std::cout << Eobs.transpose() << std::endl;
     }

     // if(robot->keyboard->getQ()) {
       //change stuff
     //}

     //if(robot->keyboard->getA()) {
       //change stuff
     //}

    PO_x = Eobs(0);
    PO_y = Eobs(1);

    Power = myPOerror(dX, Vd, Fm, dt);

    //if (abs(PO_x) > Del(0)*abs(Vd(0)-dX(0)))
    if (PO_x <= El(0))
    {
      M(0,0) = M(0,0) - alpha_x*abs(Power(0))*sign(Power(0));
      M(0,0) = max(M(0,0), 0.1);
    }
    //if (abs(PO_x) <= Del(0)*abs(Vd(0)-dX(0)))
    if (PO_x >= Eu(0))
    {M(0,0) =  max(Md(1,1), 0.1);}
    //Vd(0) = gain_x * Vd(0);

    //if (abs(PO_y) > Del(1)*abs(Vd(1)-dX(1)))
    if (PO_y <= El(1))
    {
      M(1,1) = M(1,1) - alpha_y*abs(Power(1))*sign(Power(1));
      M(1,1) = max(Md(1,1), 0.1);
    }
    //if (abs(PO_y) <= Del(1)*abs(Vd(1)-dX(1)))
    if (PO_y >= Eu(1))
    {
       //M(1,1) = M(1,1) - alpha_y*sign(PO_y);
      M(1,1) =  max(Md(1,1), 0.1);
    }
     //M(0,0) =  max(Md(0,0), 0.1);
     //M(1,1) =  max(Md(1,1), 0.1);
    Vd = myVE(X, dX, Fm, B, M, dt);

    //Vd(1) = gain_y * Vd(1);
    //apply velocity
    //robot->setEndEffVelocity();
    robot->setEndEffVelocity(Vd);
    stateLogger.recordLogData();
}

void M2Admittance3::exitCode(void) {
    robot->setJointVelocity(VM2::Zero());
}


