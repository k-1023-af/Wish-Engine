#include "DebugCamera.h"

void DebugCamera::Initialize()
{
	matRot_ = MakeIdentityMatrix();
	matRotDelta_ = MakeIdentityMatrix();
	viewMatrix_ = MakeIdentityMatrix();

	transform_ = {
		{1.0f, 1.0f, 1.0f},	//scale
		{0.0f, 0.0f, 0.0f},	//rotate
		{0, 0, -50},		//translate	
	};
}

void DebugCamera::Update()
{

	Vector3 move = { 0.0f, 0.0f, 0.0f }; //default movement
	
	//extern BYTE gKeys[256];

	//const float deltaTime = (1.0f / 60.0f);

	//if (gKeys[DIK_A]) move.x -= moveSpeed.x;	//	left	(x axis)
	//if (gKeys[DIK_D]) move.x += moveSpeed.x;	//	right	(x axis)
	//if (gKeys[DIK_W]) move.z += moveSpeed.z;	//	forward	(z axis)
	//if (gKeys[DIK_S]) move.z -= moveSpeed.z;	//	back	(z axis)
	//if (gKeys[DIK_Q]) move.y -= moveSpeed.y;	//	down	(y axis)
	//if (gKeys[DIK_E]) move.y += moveSpeed.y;	//	up		(y axis)

	///-------------------translate----------------///

	Vector3 rotatedMove = RotateVector(move, matRot_);

	if (move.x != 0.0f) { //if there is movement for right/left
		transform_.translate.x += move.x;
	}
	if (move.y != 0.0f) { //if there is movement for up/down
		transform_.translate.y += move.y;
	}
	if (move.z != 0.0f) { //input for forward/backward movement (z axis)
		transform_.translate.z += move.z;
	}
	//rotate the movement vector by the angle
	//add the coordinates by the amount of the movement vector

	///-------------------rotate----------------///

	if (rotate.x != 0.0f) { //rotation along the X-Axis
		transform_.rotate.x += rotate.x;
		//add the rotation angle of the X-Axis
	}
	
	if (rotate.y != 0.0f) { //rotation along the X-Axis
		transform_.rotate.y += rotate.y;
		//add the rotation angle of the X-Axis
	}
	
	if (rotate.z != 0.0f) { //rotation along the X-Axis
		transform_.rotate.z += rotate.z;
		//add the rotation angle of the X-Axis
	}

	////slide notes
	/*
	camera is
		Forward/Back
		Move Left/Right/Up/Down
		Rotation around each axis

		implement a switch between regular game camera and debug camera

		cumulative rotation matrix = current add rotation matrix x previous cumulative rotation matrix
	*/
	//----------

	//yaw_ -= mouseDeltaX_ * rotateSpeed.x;
	//pitch_ -= mouseDeltaY_ * rotateSpeed.y;
	//pivot_ -= mouseDeltaZ_ * rotateSpeed.z;

	transform_.rotate.x = mouseDeltaX_ * rotateSpeed.x;
	transform_.rotate.y = mouseDeltaY_ * rotateSpeed.y;
	transform_.rotate.z = mouseDeltaZ_ * rotateSpeed.z;

	if (mouseWheelDelta_ != 0.0f) {
		transform_.translate.z += mouseWheelDelta_ * 0.05f;
	}
	
	Matrix4x4 matRotX = MakeRotateXMatrix(transform_.rotate.y);
	Matrix4x4 matRotY = MakeRotateYMatrix(transform_.rotate.x);
	Matrix4x4 matRotZ = MakeRotateZMatrix(transform_.rotate.z);

	matRotDelta_ = Multiply(matRotZ, Multiply(matRotY, matRotX));

	//matRot_ = Multiply(matRotDelta, matRot_);

	//matRotDelta_ = Multiply(matRotY, matRotX);
	matRot_ = Multiply(matRotDelta_, matRot_);

	Matrix4x4 matTranslate = MakeTranslateMatrix(transform_.translate);
	Matrix4x4 worldMatrix = Multiply(matRot_, matTranslate);

	viewMatrix_ = Inverse(worldMatrix);

	////--------Projection & Viewport------------///
	//Matrix4x4 projectionMatrix = (MakePerspectiveFovMatrix(0.45f, float(kWindowWidth) / float(kWindowHeight), 0.1f, 100.f));
	//Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);
	//Matrix4x4 viewportMatrix = MakeViewportMatrix(0, 0, kWindowWidth, kWindowHeight, 0.0f, 1.0f);


	/*
	Updating the view matrix

	calculate the translation matrix from the coordinates

	calculate the world matrix from the cumulative rotation matrix and translation matrix

	assign the inverse of the world matrix into the view matrix
	
	
	*/

	mouseDeltaX_ = 0.0f;
	mouseDeltaY_ = 0.0f;
	mouseDeltaZ_ = 0.0f;
	mouseWheelDelta_ = 0.0f;
}




