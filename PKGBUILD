# Maintainer: Tiendepchai <https://github.com/Tiendepchai/swaylock-88>
pkgname=swaylock-6788-git
pkgver=1.8.4
pkgrel=2
pkgdesc="High-performance Wayland screen locker with OpenGL rendering and Elastic Typographic UI (88-series)"
arch=('x86_64' 'aarch64')
url="https://github.com/Tiendepchai/swaylock-88"
license=('MIT')
depends=('wayland' 'libxkbcommon' 'cairo' 'gdk-pixbuf2' 'pam' 'libglvnd')
makedepends=('git' 'meson' 'ninja' 'wayland-protocols' 'scdoc')
provides=('swaylock')
conflicts=('swaylock' 'swaylock-git')
source=("swaylock-88::git+https://github.com/Tiendepchai/swaylock-88.git")
md5sums=('SKIP')

pkgver() {
  cd "swaylock-88"
  printf "1.8.4.r%s.g%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
  arch-meson "swaylock-88" build
  ninja -C build
}

package() {
  DESTDIR="$pkgdir" ninja -C build install
  install -Dm644 "swaylock-88/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
