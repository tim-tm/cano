# Maintainer: CobbCoding 
pkgname=cano-git
_pkgname=cano
pkgver=r340.b5f4b53
pkgrel=1
pkgdesc="Terminal-based modal text editor"
arch=('x86_64')
url="https://github.com/CobbCoding1/cano"
license=('APACHE')
conflicts=('cano')
depends=('ncurses' 'glibc')
makedepends=('git' 'make' 'gcc' 'autoconf')
source=("$_pkgname::git+https://github.com/CobbCoding1/$_pkgname.git")
md5sums=('SKIP')

pkgver() {
    cd "$_pkgname" 
    printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
    cd "$_pkgname" 
    autoreconf -vi
    ./configure
    make
}

package() {
    cd "$_pkgname"
    make DESTDIR="$pkdir" install
    install -Dm755 ./README.md "$pkgdir/usr/share/doc/$_pkgname"
}
