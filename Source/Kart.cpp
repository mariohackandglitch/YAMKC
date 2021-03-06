#include "Kart.hpp"
#include <GL/freeglut.h>
#include <iostream>

const Vector3 Kart::defaultWheelPositions[4] = {
    Vector3(-4.524f, 2.129f, -5.311f), // Front-left
    Vector3(4.524f, 2.129f, -5.311f), // Front-right
    Vector3(-4.524f, 2.129f, 4.693f), // Back-left
    Vector3(4.524f, 2.129f, 4.693f) // Back-right
};

const Angle3 Kart::defaultWheelRotations[4] = {
    Angle3(Angle::Zero(), Angle::FromDegrees(180.f), Angle::Zero()), // Front-left
    Angle3(), // Front-right
    Angle3(Angle::Zero(), Angle::FromDegrees(180.f), Angle::Zero()), // Back-left
    Angle3() // Back-right
};

const int Kart::collisionSpherePrecision = 200;
const float Kart::collisionSphereW = 6.45f;
const float Kart::collisionSphereH = 11.45f;

const Vector3 Kart::cameraOffset = Vector3(0, 36, 51);
const Vector3 Kart::cameraLookAtOffset = Vector3(0, 23, 0);
const float Kart::cameraFov = 55.f;
const float Kart::cameraRearViewMultiplyFactor = -1.25f;
const float Kart::cameraRotationCerpFactor = 0.025f;
const float Kart::cameraPositionCerpFactor = 0.3f;

const float Kart::engineAccelerationFactor = 2500.f;
const float Kart::backwardsAccelerationRatioFactor = -0.35f;
const float Kart::wheelResistanceFactor = 2.75f;
const float Kart::windResistanceFactor = 0.0092f;
const float Kart::kartMassFactor = 8.f;
const float Kart::maxWheelTurnAngle = 19.f;
const float Kart::turnWheelFrictionFactor = 1.f;
const float Kart::turnBySpeedFactor = 210.f;
const float Kart::maxTurnAmount = 0.9f;
const float Kart::inPlaceTurnFactor = 1.1f;
const float Kart::inPlaceStartMinSpeed = 5.f;
const float Kart::wheelSpinFactor = 6.f;
const float Kart::maxWheelSpinAmount = 15.f;
const float Kart::kartBounceFactor = 350.f;
const float Kart::kartGrowFactor = 400.f;
const float Kart::offroadFactor = 20.f;

const float Kart::speedometerSpeedFactor = 0.2f;

Kart::Kart(std::string kartName, std::string wheelName, std::string driverName, std::string shadowName, Collision* col)
{
    pressedKeys = 0;
    prevPressedKeys = 0;
    tiresRotateTimer = -1;
    currCameraPos = cameraOffset;
    speed = Vector3();
    collision = col;
    advancedAmount = 0;

    try
    {
        kartObj = new Obj(kartName);
        driverObj = new Obj(driverName);
        for (int i = 0; i < 4; i++) {
            wheelObjs[i] = new Obj(wheelName);
            wheelObjs[i]->GetPosition() = defaultWheelPositions[i];
            wheelObjs[i]->GetRotation() = defaultWheelRotations[i];
        }
        shadowObj = new Obj(shadowName);
        shadowObj->GetPosition() += Vector3(0.f, 0.15f, -0.5f);
        shadowObj->GetMaterial("mat_shadow").SetRenderMode(Obj::Material::RenderMode::MULTIPLICATIVE);
    }
    catch (const char* msg) {
        std::cout << std::string("Failed to load kart: ") + msg << std::endl;
    }

    fromTireAngles[0] = wheelObjs[0]->GetRotation().y;
    fromTireAngles[1] = wheelObjs[1]->GetRotation().y;

    toTireAngles[0] = fromTireAngles[0];
    toTireAngles[1] = fromTireAngles[1];
}

Kart::~Kart()
{
    delete kartObj;
    delete driverObj;
    for (int i = 0; i < 4; i++)
        delete wheelObjs[i];
    delete shadowObj;
}

void Kart::UpdateCamera(bool birdsView)
{
    static float oldCameraRearView = cameraRearView;
    if (birdsView) {
        gluLookAt(-1300.f, 3300.f, 350.f, -1300.f, 0.f, 350 - 0.1, 0, 1, 0);
        oldCameraRearView = !cameraRearView;
    }
    else {
        static Angle3 oldRotation = GetRotation();
        
        bool shouldCerp = (oldCameraRearView == cameraRearView);

        Vector3 realCameraOffset = cameraOffset;
        realCameraOffset.z *= cameraRearView;
        Vector3 cameraPos = (GetPosition() + realCameraOffset);
        oldRotation.Cerp(GetRotation(), shouldCerp ? cameraRotationCerpFactor : 1.f);
        cameraPos.Rotate(oldRotation, GetPosition());
        currCameraPos.Cerp(cameraPos, shouldCerp ? cameraPositionCerpFactor : 1.f);
        Vector3 cameraLookAt = GetPosition() + cameraLookAtOffset;

        gluLookAt(currCameraPos.x, currCameraPos.y, currCameraPos.z, cameraLookAt.x, cameraLookAt.y, cameraLookAt.z, 0, 1, 0);
        oldCameraRearView = cameraRearView;
    }
}

void Kart::UpdateViewPort(int w, int h)
{
    float ratio = (float)w / h;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(cameraFov, ratio, 20, 10000);
}

void Kart::Draw()
{
    glPushMatrix();
    glTranslatef(position.x, position.y, position.z);
    glRotatef(rotation.x.AsDegrees(), 1.f, 0.f, 0.f);
    glRotatef(rotation.y.AsDegrees(), 0.f, 1.f, 0.f);
    glRotatef(rotation.z.AsDegrees(), 0.f, 0.f, 1.f);
    glScalef(scale.x, scale.y, scale.z);

    shadowObj->Draw();

    for (int i = 0; i < 4; i++)
        wheelObjs[i]->Draw();

    kartObj->Draw();
    driverObj->Draw();
    
    glPopMatrix();
}

void Kart::Calc(int elapsedMsec)
{
    totalElapsedTime += elapsedMsec;
    
    {
        float currSpeed = speed.Magnitude();
        kartObj->GetPosition().y = 0;
        driverObj->GetPosition().y = 0;

        if (currSpeed < kartBounceFactor) {
            float animAngleSin = Angle::FromDegrees(totalElapsedTime * 360.f / 200.f).Sin();
            kartObj->GetPosition().y = animAngleSin * 0.1f * (1.f - currSpeed / kartBounceFactor);
            shadowObj->GetScale().x = 1.05f + animAngleSin * 0.01f * (1.f - currSpeed / kartBounceFactor);
            shadowObj->GetScale().z = 0.95f + animAngleSin * 0.01f * (1.f -currSpeed / kartBounceFactor);
            driverObj->GetPosition().y = Angle::FromDegrees((totalElapsedTime + 20) * 360.f / 200.f).Sin() * 0.1f * (1.f - currSpeed / kartBounceFactor);
        }
        float realGrowFactor = currSpeed / kartGrowFactor;
        if (realGrowFactor > 1.f)
            realGrowFactor = 1.f;
        kartObj->GetScale().y = (1.f + realGrowFactor * 0.1f);
        kartObj->GetScale().z = (1.f + realGrowFactor * 0.1f);
    }

    if ((prevPressedKeys & (unsigned int)Key::KEY_Left) != (pressedKeys & (unsigned int)Key::KEY_Left) || (prevPressedKeys & (unsigned int)Key::KEY_Right) != (pressedKeys & (unsigned int)Key::KEY_Right))
    {
        tiresRotateTimer = 0;
        fromTireAngles[0] = wheelObjs[0]->GetRotation().y;
        fromTireAngles[1] = wheelObjs[1]->GetRotation().y;
        if (pressedKeys & (unsigned int)Key::KEY_Left) {
            toTireAngles[0] = defaultWheelRotations[0].y + Angle::FromDegrees(maxWheelTurnAngle);
            toTireAngles[1] = defaultWheelRotations[1].y + Angle::FromDegrees(maxWheelTurnAngle);
        }
        else if (pressedKeys & (unsigned int)Key::KEY_Right) {
            toTireAngles[0] = defaultWheelRotations[0].y - Angle::FromDegrees(maxWheelTurnAngle);
            toTireAngles[1] = defaultWheelRotations[1].y - Angle::FromDegrees(maxWheelTurnAngle);
        }
        else {
            toTireAngles[0] = defaultWheelRotations[0].y;
            toTireAngles[1] = defaultWheelRotations[1].y;
        }
    }

    Angle left = Angle::Zero();
    Angle right = Angle::Zero();

    if (tiresRotateTimer >= 0 && tiresRotateTimer < 200) {
        left = fromTireAngles[0];
        right = fromTireAngles[1];
        left.Cerp(toTireAngles[0], tiresRotateTimer / 200.f);
        right.Cerp(toTireAngles[1], tiresRotateTimer / 200.f);

        tiresRotateTimer += elapsedMsec;
    }
    else
    {
        left = toTireAngles[0];
        right = toTireAngles[1];
        tiresRotateTimer = -1;
    }

    Vector3 forward = GetForward();
    Vector3 speedVector = speed;
    float realAccelFactor = 0.f;
    float realMassFactor = kartMassFactor;
    bool inPlaceDrift = false;

    
    if (pressedKeys & (unsigned int)Key::KEY_B) {
        realAccelFactor = engineAccelerationFactor * backwardsAccelerationRatioFactor;
    } else if (pressedKeys & (unsigned int)Key::KEY_A) {
        realAccelFactor = engineAccelerationFactor;
        realMassFactor *= 1.2f;
    }

    if (pressedKeys & (unsigned int)Key::KEY_B && pressedKeys & (unsigned int)Key::KEY_A && speed.Magnitude() < inPlaceStartMinSpeed) {
        inPlaceDrift = true;
        speed = Vector3(0.f, 0.f, 0.f);
    }

    Color currentCol = collision->GetColorAtPosition(GetPosition());

    Vector3 engineForce = forward * realAccelFactor;
    float realWheelResistanceFactor = wheelResistanceFactor + currentCol.r * offroadFactor;
    Vector3 tireResistance = speedVector * -(realWheelResistanceFactor + (abs(right.AsDegrees()) / maxWheelTurnAngle) * turnWheelFrictionFactor);
    Vector3 windResistance = speedVector * (-windResistanceFactor * speedVector.Magnitude());
    Vector3 totalAcceleration = (engineForce + tireResistance + windResistance) / realMassFactor;

    if (!inPlaceDrift) speed += (totalAcceleration * (elapsedMsec / 1000.f));

    Vector3 advanceNow;
    float wheelSpinAmount = 0.f;

    if (inPlaceDrift) {
        Angle rotateAngle = Angle::FromDegrees((right.AsDegrees() / maxWheelTurnAngle) * inPlaceTurnFactor);
        wheelSpinAmount = maxWheelSpinAmount;
        GetRotation().y += rotateAngle;
    }
    else {
        Point speedPoint(0.f);
        Point maxPoint(maxTurnAmount);
        speedPoint.Cerp(maxPoint, speed.Magnitude() / turnBySpeedFactor);

        Angle rotateAngle = Angle::FromDegrees(right.AsDegrees() / maxWheelTurnAngle) * speedPoint.value;
        speed.Rotate(Angle3(Angle::Zero(), rotateAngle, Angle::Zero()));
        advanceNow = speed * (elapsedMsec / 1000.f);
        Angle angleWithFwd = forward.Angle(advanceNow);
        bool goingBackwards = false;
        if (!std::isinf(angleWithFwd.AsDegrees()) && abs(angleWithFwd.AsDegrees()) > 90.f)
        {
            speed.Rotate(Angle3(Angle::Zero(), rotateAngle * -2.f, Angle::Zero()));
            advanceNow = advanceNow * -1.f;
            goingBackwards = true;
        }

        Angle kartAngle = rotateAngle * -2.f;
        float absoluteAngle = abs(kartAngle.AsDegrees()) / 3.5f;
        kartObj->GetRotation().z = kartAngle;
        driverObj->GetRotation().z = kartObj->GetRotation().z;
        kartObj->GetPosition().y += absoluteAngle;
        driverObj->GetPosition().y += absoluteAngle;
        wheelObjs[0]->GetRotation().y = rotateAngle * maxWheelTurnAngle + defaultWheelRotations[0].y;
        wheelObjs[1]->GetRotation().y = rotateAngle * maxWheelTurnAngle + defaultWheelRotations[1].y;

        Angle newAngle = Vector3(0.f, 0.f, -1.f).Angle(advanceNow);
        if (!std::isinf(newAngle.AsRadians()) && abs(advanceNow.Angle(forward).AsDegrees()) < 90.f)
            GetRotation().y = newAngle;

        Vector3 newKartPosition = CalcCollision(advanceNow * (goingBackwards ? -1.f : 1.f));
        wheelSpinAmount = (newKartPosition - GetPosition()).Magnitude() * wheelSpinFactor * (goingBackwards ? -1.f : 1.f);

        advancedAmount = (newKartPosition - GetPosition()).Magnitude();
        if (abs((newKartPosition - GetPosition()).Angle(GetForward()).AsDegrees()) > 90.f || goingBackwards)
            advancedAmount = 0.f;
        GetPosition() = newKartPosition;
    }

    if (abs(wheelSpinAmount) > maxWheelSpinAmount)
        wheelSpinAmount = maxWheelSpinAmount * copysign(1.f, wheelSpinAmount);
    for (int i = 0; i < 4; i++) {
        wheelObjs[i]->GetPreRotation().x += Angle::FromDegrees(wheelSpinAmount * ((i & 1) ? -1.f : 1.f));
    }

    if (pressedKeys & (unsigned int)Key::KEY_X)
        cameraRearView = cameraRearViewMultiplyFactor;
    else
        cameraRearView = 1.f;

    prevPressedKeys = pressedKeys;
}

void Kart::KeyPress(Key key)
{
    pressedKeys |= (unsigned int)key;
}

void Kart::KeyRelease(Key key)
{
    pressedKeys &= ~(unsigned int)key;
}

Angle Kart::GetSpeedometerAngle(int elapsedMsec)
{
    float angle = (advancedAmount / (elapsedMsec / 1000.f)) * speedometerSpeedFactor;
    if (angle > 90.f)
        angle = 90.f;
    return Angle::FromDegrees(angle);
}

Vector3& Kart::GetPosition()
{
    return position;
}

Angle3& Kart::GetRotation()
{
    return rotation;
}

Vector3& Kart::GetScale()
{
    return scale;
}

Vector3 Kart::GetForward()
{
    Vector3 unit(0, 0, -1);
    unit.Rotate(GetRotation());
    unit.Normalize();
    return unit;
}

Obj* Kart::GetDriverObj()
{
    return driverObj;
}

Obj* Kart::GetKartObj()
{
    return kartObj;
}

Obj** Kart::GetWheelObjs()
{
    return wheelObjs;
}

unsigned int Kart::KeysJustPressed()
{
    return (pressedKeys ^ prevPressedKeys) & pressedKeys;
}

Vector3 Kart::CalcCollision(const Vector3& advancePos)
{
    Vector3 newAdvancePos = advancePos;

    for (int i = 0; i < collisionSpherePrecision; i++) {
        Vector3 currentPoint;
        Angle currAngle = Angle::FromDegrees((float)i / collisionSpherePrecision * 360.f);
        currentPoint.x = currAngle.Sin() * collisionSphereW;
        currentPoint.z = currAngle.Cos() * collisionSphereH;
        currentPoint += GetPosition();
        currentPoint.Rotate(GetRotation(), GetPosition());

        for (int j = 0; j < 4; j++) {
            Collision::WallType w = collision->GetWallTypeAtPosition(currentPoint + newAdvancePos);
            switch (w)
            {
            case Collision::WallType::NONE:
                break;
            case Collision::WallType::SOUTH:
                if (newAdvancePos.z < 0.f)
                    newAdvancePos.z = 0.f;
                break;
            case Collision::WallType::WEST:
                if (newAdvancePos.x > 0.f)
                    newAdvancePos.x = 0.f;
                break;
            case Collision::WallType::NORTH:
                if (newAdvancePos.z > 0.f)
                    newAdvancePos.z = 0.f;
                break;
            case Collision::WallType::EAST:
                if (newAdvancePos.x < 0.f)
                    newAdvancePos.x = 0.f;
                break;
            case Collision::WallType::ALL:
                newAdvancePos = Vector3();
                break;
            default:
                break;
            }
        }
    }
    return GetPosition() + newAdvancePos;
}
