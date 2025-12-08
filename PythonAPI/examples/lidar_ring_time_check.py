#!/usr/bin/env python3
# Subscribe to ROS2 PointCloud2 topic to collect lidar data, parse ring/time fields, visualize and export PLY.

import argparse
import time
import collections
import struct

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import PointCloud2
import matplotlib.pyplot as plt
import numpy as np


class LidarSubscriber(Node):
	"""ROS2 subscriber for PointCloud2 lidar data"""
	
	def __init__(self, topic_name, collect_duration=1.0):
		super().__init__('lidar_ring_time_check_subscriber')
		self.topic_name = topic_name
		self.collect_duration = collect_duration
		self.all_messages = []
		self.start_time = None
		self.end_time = None
		
		self.subscription = self.create_subscription(
			PointCloud2,
			self.topic_name,
			self.lidar_callback,
			10
		)
		self.get_logger().info(f'Subscribing to topic: {self.topic_name}')
	
	def lidar_callback(self, msg):
		"""Callback function for PointCloud2 messages"""
		if self.start_time is None:
			self.start_time = time.time()
			self.end_time = self.start_time + self.collect_duration
			self.get_logger().info(f'Started collecting data for {self.collect_duration} seconds...')
		
		current_time = time.time()
		if current_time <= self.end_time:
			self.all_messages.append(msg)
			self.get_logger().info(
				f'Received message #{len(self.all_messages)}: {msg.width} points, '
				f'point_step={msg.point_step}, time={current_time - self.start_time:.3f}s'
			)
		else:
			# Stop collecting after duration
			pass
	
	def is_collection_complete(self):
		"""Check if collection duration has elapsed"""
		if self.start_time is None:
			return False
		return time.time() >= self.end_time
	
	def get_collected_data(self):
		"""Get all collected messages and actual duration"""
		if self.start_time is None:
			return None, 0.0
		actual_duration = time.time() - self.start_time
		return self.all_messages, actual_duration


def _parse_pointcloud2_messages(messages):
	"""Parse PointCloud2 messages and extract point data"""
	if not messages:
		return None, 0
	
	all_points = []
	total_points = 0
	
	for msg in messages:
		# Get field offsets
		fields_dict = {field.name: field for field in msg.fields}
		
		# Verify required fields exist
		required_fields = ['x', 'y', 'z', 'intensity', 'ring', 'time']
		missing_fields = [f for f in required_fields if f not in fields_dict]
		if missing_fields:
			print(f"WARNING: Missing fields in PointCloud2: {missing_fields}")
			continue
		
		# Extract field offsets
		x_offset = fields_dict['x'].offset
		y_offset = fields_dict['y'].offset
		z_offset = fields_dict['z'].offset
		intensity_offset = fields_dict['intensity'].offset
		ring_offset = fields_dict['ring'].offset
		time_offset = fields_dict['time'].offset
		point_step = msg.point_step
		
		# Parse points
		points_in_msg = []
		for i in range(msg.width):
			offset = i * point_step
			
			# Extract data using struct
			x = struct.unpack_from('f', msg.data, offset + x_offset)[0]
			y = struct.unpack_from('f', msg.data, offset + y_offset)[0]
			z = struct.unpack_from('f', msg.data, offset + z_offset)[0]
			intensity = struct.unpack_from('f', msg.data, offset + intensity_offset)[0]
			ring = struct.unpack_from('H', msg.data, offset + ring_offset)[0]  # 'H' = uint16
			point_time = struct.unpack_from('f', msg.data, offset + time_offset)[0]
			
			points_in_msg.append((x, y, z, intensity, ring, point_time))
		
		all_points.extend(points_in_msg)
		total_points += msg.width
	
	# Convert to numpy structured array
	if not all_points:
		return None, 0
	
	dtype = np.dtype([
		('x', 'f4'), ('y', 'f4'), ('z', 'f4'),
		('intensity', 'f4'),
		('ring', 'u2'),
		('time', 'f4'),
	])
	
	points_array = np.array(all_points, dtype=dtype)
	return points_array, total_points


def _visualize(points: np.ndarray, duration: float):
	"""Visualize point cloud data, focusing on ring and time validation"""
	total_points = len(points)
	points_per_second = total_points / duration if duration > 0 else 0
	
	print(f"\n=== Data Statistics ===")
	print(f"Total Points: {total_points:,}")
	print(f"Collection Duration: {duration:.3f} seconds")
	print(f"Points Per Second: {points_per_second:,.0f} points/sec")
	print(f"Point Size: {points.dtype.itemsize} bytes (expected 24)")
	
	# Ring statistics
	unique_rings = np.unique(points["ring"])
	ring_counts = collections.Counter(points["ring"])
	print(f"\n=== Ring Statistics ===")
	print(f"Ring Range: {unique_rings.min()} - {unique_rings.max()}")
	print(f"Ring Count: {len(unique_rings)} unique rings")
	print(f"Ring Distribution:")
	for ring in sorted(ring_counts.keys()):
		print(f"  Ring {ring:2d}: {ring_counts[ring]:,} points ({ring_counts[ring]/total_points*100:.1f}%)")
	
	# Time statistics
	time_min = points["time"].min()
	time_max = points["time"].max()
	time_mean = points["time"].mean()
	print(f"\n=== Time Statistics ===")
	print(f"Time Range: {time_min:.6f} - {time_max:.6f} seconds")
	print(f"Time Mean: {time_mean:.6f} seconds")
	print(f"Time Span: {time_max - time_min:.6f} seconds")
	
	# Create visualizations
	fig = plt.figure(figsize=(18, 10))
	
	# 1. Ring histogram
	ax1 = plt.subplot(2, 3, 1)
	ax1.hist(points["ring"], bins=max(unique_rings.max() + 1, 32), edgecolor="k", alpha=0.7)
	ax1.set_title(f"Ring Histogram ({len(unique_rings)} rings)")
	ax1.set_xlabel("Ring Number")
	ax1.set_ylabel("Point Count")
	ax1.grid(True, alpha=0.3)
	
	# 2. Time distribution histogram
	ax2 = plt.subplot(2, 3, 2)
	ax2.hist(points["time"], bins=50, edgecolor="k", alpha=0.7)
	ax2.set_title("Time Distribution Histogram")
	ax2.set_xlabel("Time (seconds)")
	ax2.set_ylabel("Point Count")
	ax2.grid(True, alpha=0.3)
	
	# 3. Time vs Point Index (sample first 10000 points to avoid overcrowding)
	ax3 = plt.subplot(2, 3, 3)
	sample_size = min(10000, total_points)
	sample_indices = np.linspace(0, total_points - 1, sample_size, dtype=int)
	ax3.scatter(sample_indices, points["time"][sample_indices], s=1, alpha=0.5)
	ax3.set_title(f"Time vs Point Index (sampled {sample_size} points)")
	ax3.set_xlabel("Point Index")
	ax3.set_ylabel("Time (seconds)")
	ax3.grid(True, alpha=0.3)
	
	# 4. Ring vs Time scatter plot (sampled)
	ax4 = plt.subplot(2, 3, 4)
	sample_size = min(5000, total_points)
	sample_indices = np.random.choice(total_points, sample_size, replace=False)
	scatter = ax4.scatter(points["time"][sample_indices], points["ring"][sample_indices], 
	                     c=points["intensity"][sample_indices], s=10, alpha=0.6, cmap="viridis")
	ax4.set_title(f"Ring vs Time (sampled {sample_size} points, color=intensity)")
	ax4.set_xlabel("Time (seconds)")
	ax4.set_ylabel("Ring Number")
	ax4.grid(True, alpha=0.3)
	plt.colorbar(scatter, ax=ax4, label="Intensity")
	
	# 5. Top-down view (x-y plane)
	ax5 = plt.subplot(2, 3, 5)
	sample_size = min(10000, total_points)
	sample_indices = np.random.choice(total_points, sample_size, replace=False)
	sc = ax5.scatter(points["x"][sample_indices], points["y"][sample_indices], 
	                c=points["ring"][sample_indices], s=2, alpha=0.6, cmap="tab20")
	ax5.set_title(f"Top-down View (x-y plane, sampled {sample_size} points, color=ring)")
	ax5.set_xlabel("X (meters)")
	ax5.set_ylabel("Y (meters)")
	ax5.axis("equal")
	ax5.grid(True, alpha=0.3)
	plt.colorbar(sc, ax=ax5, label="Ring")
	
	# 6. Time distribution boxplot for each ring
	ax6 = plt.subplot(2, 3, 6)
	ring_time_data = []
	ring_labels = []
	for ring in sorted(unique_rings):
		ring_mask = points["ring"] == ring
		ring_times = points["time"][ring_mask]
		if len(ring_times) > 0:
			ring_time_data.append(ring_times)
			ring_labels.append(f"R{ring}")
	
	if ring_time_data:
		bp = ax6.boxplot(ring_time_data, labels=ring_labels, vert=True, patch_artist=True)
		ax6.set_title("Time Distribution Boxplot by Ring")
		ax6.set_xlabel("Ring Number")
		ax6.set_ylabel("Time (seconds)")
		ax6.tick_params(axis='x', rotation=45)
		ax6.grid(True, alpha=0.3)
	
	plt.tight_layout()
	plt.show()
	
	# Validate ring and time correctness
	print(f"\n=== Data Validation ===")
	
	# Check if rings are continuous
	expected_rings = set(range(int(unique_rings.min()), int(unique_rings.max()) + 1))
	actual_rings = set(unique_rings)
	missing_rings = expected_rings - actual_rings
	if missing_rings:
		print(f"WARNING: Missing rings {sorted(missing_rings)}")
	else:
		print(f"OK: Rings are continuous from {unique_rings.min()} to {unique_rings.max()}")
	
	# Check if time is monotonically increasing
	time_sorted = np.sort(points["time"])
	time_diff = np.diff(time_sorted)
	if np.any(time_diff < 0):
		print(f"WARNING: Time data has duplicates or is out of order")
	else:
		print(f"OK: Time data is monotonically increasing")
	
	# Check if time range is reasonable (should be 0 to scan period)
	rotation_freq = 10.0  # Default 10 Hz, adjust based on actual configuration
	scan_period = 1.0 / rotation_freq
	if time_max > scan_period * 2:
		print(f"WARNING: Time max ({time_max:.6f}) may exceed expected range (scan period ~{scan_period:.3f} sec)")
	else:
		print(f"OK: Time range is reasonable (0 - {time_max:.6f} sec, scan period ~{scan_period:.3f} sec)")


def _export_ply(points: np.ndarray, path: str):
	"""Export PLY file"""
	with open(path, "w", encoding="utf-8") as f:
		f.write("ply\nformat ascii 1.0\n")
		f.write(f"element vertex {len(points)}\n")
		f.write("property float x\nproperty float y\nproperty float z\n")
		f.write("property float intensity\nproperty ushort ring\nproperty float time\n")
		f.write("end_header\n")
		for p in points:
			f.write(f"{p['x']} {p['y']} {p['z']} {p['intensity']} {p['ring']} {p['time']}\n")
	print(f"Exported PLY: {path}")


def main():
	parser = argparse.ArgumentParser(description="Subscribe to ROS2 PointCloud2 topic and validate ring/time fields")
	parser.add_argument("--topic", default="/carla/robot01/sensor/lidar/points", 
	                   help="ROS2 topic name for PointCloud2")
	parser.add_argument("--duration", type=float, default=1.0, 
	                   help="Data collection duration in seconds")
	parser.add_argument("--ply", default="lidar_one_second.ply", 
	                   help="Output PLY file path")
	parser.add_argument("--robot-name", default="robot01",
	                   help="Robot name (used to construct topic name if --topic not provided)")
	args = parser.parse_args()
	
	# Initialize ROS2
	rclpy.init()
	
	# Create subscriber node
	subscriber = LidarSubscriber(args.topic, args.duration)
	
	print(f"Waiting for messages on topic: {args.topic}")
	print(f"Collection duration: {args.duration} seconds")
	print("Press Ctrl+C to stop early")
	
	# Spin until collection is complete
	try:
		start_time = time.time()
		while not subscriber.is_collection_complete():
			rclpy.spin_once(subscriber, timeout_sec=0.1)
			# Check timeout (safety)
			if time.time() - start_time > args.duration + 5.0:
				print("Timeout: Stopping collection")
				break
	except KeyboardInterrupt:
		print("\nInterrupted by user")
	finally:
		# Get collected data
		messages, actual_duration = subscriber.get_collected_data()
		
		if not messages:
			print("No messages received. Please check:")
			print(f"  1. Topic name: {args.topic}")
			print("  2. ROS2 node is running and publishing")
			print("  3. Topic exists: ros2 topic list | grep lidar")
			rclpy.shutdown()
			return
		
		print(f"\nCollection complete!")
		print(f"Received {len(messages)} messages")
		print(f"Actual duration: {actual_duration:.3f} seconds")
		
		# Parse messages
		points, total_points = _parse_pointcloud2_messages(messages)
		
		if points is None or len(points) == 0:
			print("Failed to parse point cloud data.")
			rclpy.shutdown()
			return
		
		# Calculate points per second
		points_per_second = len(points) / actual_duration if actual_duration > 0 else 0
		print(f"Total points: {len(points):,}")
		print(f"Points per second: {points_per_second:,.0f} points/sec")
		
		# Visualize
		_visualize(points, actual_duration)
		
		# Export PLY
		_export_ply(points, args.ply)
		
		# Cleanup
		subscriber.destroy_node()
		rclpy.shutdown()


if __name__ == "__main__":
	main()

