import numpy as np

# Original projection parameters
a = [1.2, -0.8, 0.05, 2.0]  # a1, a2, a3, a4
b = [0.9, 1.1, -0.03, -1.5]  # b1, b2, b3, b4

def forward_project(vx, vy):
    px = a[0] * vx + a[1] * vy + a[2] * vx * vy + a[3]
    py = b[0] * vx + b[1] * vy + b[2] * vx * vy + b[3]
    return px, py

# Generate synthetic data
np.random.seed(0)
N = 1000
vx_data = np.random.uniform(-10, 10, N)
vy_data = np.random.uniform(-10, 10, N)
px_data, py_data = forward_project(vx_data, vy_data)

# Initialize inverse projection parameters: vx = c1*px + c2*py + c3*px*py + c4
c = np.random.randn(4)
d = np.random.randn(4)

# SGD settings
lr = 1e-5
epochs = 5000
batch_size = 64

for epoch in range(epochs):
    # Shuffle
    idx = np.random.permutation(N)
    px_data, py_data, vx_data, vy_data = px_data[idx], py_data[idx], vx_data[idx], vy_data[idx]

    for i in range(0, N, batch_size):
        px_batch = px_data[i:i+batch_size]
        py_batch = py_data[i:i+batch_size]
        vx_true = vx_data[i:i+batch_size]
        vy_true = vy_data[i:i+batch_size]

        # Predicted vx, vy
        vx_pred = c[0] * px_batch + c[1] * py_batch + c[2] * px_batch * py_batch + c[3]
        vy_pred = d[0] * px_batch + d[1] * py_batch + d[2] * px_batch * py_batch + d[3]

        # Loss: MSE
        loss = np.mean((vx_pred - vx_true)**2 + (vy_pred - vy_true)**2)

        # Gradients
        dL_dvx = 2 * (vx_pred - vx_true) / batch_size
        dL_dvy = 2 * (vy_pred - vy_true) / batch_size

        grad_c = np.array([
            np.sum(dL_dvx * px_batch),
            np.sum(dL_dvx * py_batch),
            np.sum(dL_dvx * px_batch * py_batch),
            np.sum(dL_dvx),
        ])

        grad_d = np.array([
            np.sum(dL_dvy * px_batch),
            np.sum(dL_dvy * py_batch),
            np.sum(dL_dvy * px_batch * py_batch),
            np.sum(dL_dvy),
        ])

        # Update
        c -= lr * grad_c
        d -= lr * grad_d

    if epoch % 500 == 0 or epoch == epochs - 1:
        print(f"Epoch {epoch}, Loss: {loss:.6f}")

print("\nLearned inverse projection coefficients:")
print(f"c1–4: {c}")
print(f"d1–4: {d}")