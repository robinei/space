#ifndef FPSCAMERA_H
#define FPSCAMERA_H

class FPSCamera {
public:
	void set_pos(vec3 pos) {
		_pos = pos;
	}

	void set_dir(vec3 dir) {
		_dir = dir;
	}

	void look_at(vec3 target) {
		_dir = glm::normalize(target - _pos);
	}

	vec3 pos() {
		return _pos;
	}

	vec3 dir() {
		return _dir;
	}

	void step(float amount) {
		_pos += _dir * amount;
	}

	void strafe(float amount) {
		vec3 side = glm::normalize(glm::cross(_dir, vec3(0, 1, 0)));
		_pos += side * amount;
	}

	void rise(float amount) {
		vec3 side = glm::normalize(glm::cross(_dir, vec3(0, 1, 0)));
		vec3 up = glm::normalize(glm::cross(side, _dir));
		_pos += up * amount;
	}

	void rotate(float yaw, float pitch) {
		vec3 up = vec3(0, 1, 0);
		vec3 side = glm::normalize(glm::cross(_dir, up));
		_dir = glm::angleAxis(yaw, up) * _dir;
		_dir = glm::angleAxis(pitch, side) * _dir;
	}

	mat4 view_matrix() {
		return glm::lookAt(
			_pos,
			_pos + _dir,
			vec3(0, 1, 0)
			);
	}

private:
	vec3 _pos;
	vec3 _dir;
};

#endif
