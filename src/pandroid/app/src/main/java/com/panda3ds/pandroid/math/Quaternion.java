package com.panda3ds.pandroid.math;

public class Quaternion {
	public float x, y, z, w;
	public Quaternion(float x, float y, float z, float w) {
		this.x = x;
		this.y = y;
		this.z = z;
		this.w = w;
	}

	public Quaternion fromEuler(Vector3 euler) {
		float x = euler.x;
		float y = euler.y;
		float z = euler.z;

		double c1 = Math.cos(x / 2.0);
		double c2 = Math.cos(y / 2.0);
		double c3 = Math.cos(z / 2.0);

		double s1 = Math.sin(x / 2.0);
		double s2 = Math.sin(y / 2.0);
		double s3 = Math.sin(z / 2.0);

		this.x = (float) (s1 * c2 * c3 + c1 * s2 * s3);
		this.y = (float) (c1 * s2 * c3 - s1 * c2 * s3);
		this.z = (float) (c1 * c2 * s3 + s1 * s2 * c3);
		this.w = (float) (c1 * c2 * c3 - s1 * s2 * s3);
		return this;
	}
}
