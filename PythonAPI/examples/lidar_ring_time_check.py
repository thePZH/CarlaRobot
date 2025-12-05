#!/usr/bin/env python3
# 通过 CARLA 采集一帧激光数据，解析 24 字节的 LidarDetection（含 ring/time），绘制可视化并导出 PLY。

import argparse
import time

import carla
import matplotlib.pyplot as plt
import numpy as np


# LidarDetection 布局：x,y,z,intensity (float32) + ring(uint16) + padding(uint16) + time(float32) = 24 bytes
_DETECTION_DTYPE = np.dtype([
	("x", "f4"), ("y", "f4"), ("z", "f4"),
	("intensity", "f4"),
	("ring", "u2"), ("padding", "u2"),
	("time", "f4"),
])


def _find_existing_lidar(world: carla.World) -> carla.Actor | None:
	actors = world.get_actors().filter("sensor.lidar.ray_cast*")
	return actors[0] if len(actors) > 0 else None


def _collect_one_frame(lidar: carla.Actor, world: carla.World, timeout_sec: int) -> bytes | None:
	frame = {"raw": None}

	def _callback(measurement: carla.LidarMeasurement):
		frame["raw"] = measurement.raw_data

	lidar.listen(_callback)
	deadline = time.time() + timeout_sec
	while frame["raw"] is None and time.time() < deadline:
		world.wait_for_tick()
	lidar.stop()
	return frame["raw"]


def _visualize(points: np.ndarray):
	print(f"点数: {len(points)}, dtype size: {points.dtype.itemsize} (应为 24)")

	fig, axes = plt.subplots(1, 3, figsize=(16, 4))

	axes[0].hist(points["ring"], bins=64, edgecolor="k")
	axes[0].set_title("Ring 直方图")
	axes[0].set_xlabel("ring")
	axes[0].set_ylabel("count")

	axes[1].scatter(np.arange(len(points)), points["time"], s=1, alpha=0.4)
	axes[1].set_title("Time vs point index")
	axes[1].set_xlabel("point index")
	axes[1].set_ylabel("time (s)")

	# 俯视图（忽略 Z），用强度着色，ring 作为大小区分
	sc = axes[2].scatter(points["x"], points["y"], c=points["intensity"], s=2, alpha=0.6, cmap="viridis")
	axes[2].set_title("Top-down (x,y) colored by intensity")
	axes[2].set_xlabel("x")
	axes[2].set_ylabel("y")
	axes[2].axis("equal")
	cb = fig.colorbar(sc, ax=axes[2])
	cb.set_label("intensity")

	plt.tight_layout()
	plt.show()


def _export_ply(points: np.ndarray, path: str):
	with open(path, "w", encoding="utf-8") as f:
		f.write("ply\nformat ascii 1.0\n")
		f.write(f"element vertex {len(points)}\n")
		f.write("property float x\nproperty float y\nproperty float z\n")
		f.write("property float intensity\nproperty ushort ring\nproperty float time\n")
		f.write("end_header\n")
		for p in points:
			f.write(f"{p['x']} {p['y']} {p['z']} {p['intensity']} {p['ring']} {p['time']}\n")
	print(f"已写入 PLY: {path}")


def main():
	parser = argparse.ArgumentParser(description="验证 Lidar ring/time 字段并可视化")
	parser.add_argument("--host", default="127.0.0.1")
	parser.add_argument("--port", type=int, default=2000)
	parser.add_argument("--wait", type=int, default=5, help="等待一帧的秒数")
	parser.add_argument("--ply", default="lidar_frame.ply", help="导出 PLY 路径")
	args = parser.parse_args()

	client = carla.Client(args.host, args.port)
	client.set_timeout(5.0)
	world = client.get_world()

	lidar = _find_existing_lidar(world)
	if lidar is None:
		print("未找到已存在的激光雷达，请先在仿真中生成激光雷达。")
		return

	print(f"使用现有激光雷达: id={lidar.id}, name={lidar.type_id}")
	print("等待一帧激光数据...")
	raw = _collect_one_frame(lidar, world, args.wait)
	if raw is None:
		print("未收到激光帧，请检查传感器或等待时间。")
		return

	points = np.frombuffer(raw, dtype=_DETECTION_DTYPE)
	_visualize(points)
	_export_ply(points, args.ply)


if __name__ == "__main__":
	main()

