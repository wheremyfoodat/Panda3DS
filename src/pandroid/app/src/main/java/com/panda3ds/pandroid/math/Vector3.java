package com.panda3ds.pandroid.math;

public class Vector3 {
	private final Quaternion quaternion = new Quaternion(0, 0, 0, 0);
	public float x, y, z;

	public Vector3(float x, float y, float z) {
		this.x = x;
		this.y = y;
		this.z = z;
	}

	public Vector3 rotateByEuler(Vector3 euler) {
		this.quaternion.fromEuler(euler);

		float x = this.x, y = this.y, z = this.z;
		float qx = this.quaternion.x;
		float qy = this.quaternion.y;
		float qz = this.quaternion.z;
		float qw = this.quaternion.w;

		float ix = qw * x + qy * z - qz * y;
		float iy = qw * y + qz * x - qx * z;
		float iz = qw * z + qx * y - qy * x;
		float iw = -qx * x - qy * qz * z;

		this.x = ix * qw + iw * -qx + iy * -qz - iz * -qy;
		this.y = iy * qw + iw * -qy + iz * -qx - ix * -qz;
		this.z = iz * qw + iw * -qz + ix * -qy - iy * -qx;
		return this;
	}
}
