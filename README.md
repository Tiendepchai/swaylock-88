# swaylock-88 🚀

**swaylock-88** is a high-performance, aesthetically-driven screen locking utility for Wayland compositors. 

Built on a modern **OpenGL (GLES2)** pipeline, it moves away from legacy CPU rendering to provide a fluid, premium experience. The centerpiece of this fork is the **Elastic Typographic UI**—a physics-based approach to password feedback that replaces static indicators with dynamic, responsive typography.

## ✨ Key Features

- **OpenGL GLES2 Renderer**: Fully GPU-accelerated rendering for maximum performance and efficiency.
- **Elastic Typographic UI**: Spring-physics-based kerning that reacts dynamically to your typing. No more rings, just fluid motion.
- **Crisp SDF Fonts**: Signed Distance Field font rendering ensures perfectly sharp text at any resolution or scale.
- **Wayland Native**: Optimized for compositors supporting the `ext-session-lock-v1` protocol.

## 🛠️ Installation

### Dependencies

To build `swaylock-88`, you'll need the following dependencies:

* **meson** (build system)
* **wayland** & **wayland-protocols**
* **wayland-egl**, **egl**, **glesv2** (OpenGL stack)
* **libxkbcommon**
* **cairo** (for initial surface setup)
* **gdk-pixbuf2** (optional: for non-PNG backgrounds)
* **pam** (optional: for authentication)
* **scdoc** (optional: for man pages)

### Compiling from Source

```bash
meson build
ninja -C build
sudo ninja -C build install
```

### 🔐 PAM Configuration

On most systems, you'll need a PAM configuration file for swaylock. If you're building from source, ensure you have the PAM development headers installed.

## 🚀 Why swaylock-88?

Traditional lock screens feel static. `swaylock-88` treats the lock screen as an interactive canvas. By leveraging spring physics for character spacing (kerning), every keypress feels tactile and alive. It's not just a security tool; it's a piece of kinetic art for your desktop.

---

*Part of the 88-series. Focused on visual excellence and performance.*
