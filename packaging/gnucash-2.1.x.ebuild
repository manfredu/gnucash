# Copyright 1999-2006 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header$

# This script should work fine for the whole 2.1.x (and hopefully 2.2.x)
# releases with a simple rename. See
# http://bugs.gentoo.org/show_bug.cgi?id=122337 for discussion and history
# about this file.  

# -- jsled-gentoo@asynchronous.org

inherit eutils gnome2 

DOC_VER="2.0.1"

DESCRIPTION="A personal finance manager (unstable version)."
HOMEPAGE="http://www.gnucash.org/"
SRC_URI="mirror://sourceforge/gnucash/${P}.tar.gz
         mirror://sourceforge/gnucash/gnucash-docs-${DOC_VER}.tar.gz"
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE="ofx hbci chipcard doc debug quotes nls"
# mt940

RDEPEND=">=dev-libs/glib-2.6.3
	>=dev-scheme/guile-1.8
	=dev-scheme/slib-3.1.1*
	>=sys-libs/zlib-1.1.4
	>=dev-libs/popt-1.5
	>=x11-libs/gtk+-2.6
	>=gnome-base/libgnomeui-2.4
	>=gnome-base/libgnomeprint-2.10
	>=gnome-base/libgnomeprintui-2.10
	>=gnome-base/libglade-2.4
	>=gnome-extra/gtkhtml-3.10.1
	>=dev-libs/libxml2-2.5.10
	>=gnome-base/gconf-2
	>=app-text/scrollkeeper-0.3
	>=x11-libs/goffice-0.1.0
	ofx? ( >=dev-libs/libofx-0.7.0 )
	hbci? ( net-libs/aqbanking
		chipcard? ( sys-libs/libchipcard )
	)
	quotes? ( dev-perl/DateManip
		dev-perl/Finance-Quote-1.11
		dev-perl/HTML-TableExtract )
	app-text/docbook-xsl-stylesheets
	=app-text/docbook-xml-dtd-4.1.2*
	nls? ( dev-util/intltool )
	media-libs/libart_lgpl
	x11-libs/pango"

DEPEND="${RDEPEND}
	doc? ( app-doc/doxygen
			media-gfx/graphviz
			virtual/tetex )
	dev-util/pkgconfig
	sys-devel/libtool
	>=dev-lang/swig-1.3.28"

S="${S}."

pkg_setup() {
	built_with_use gnome-extra/libgsf gnome || die "gnome-extra/libgsf must be built with gnome"
	built_with_use x11-libs/goffice gnome || die "x11-libs/goffice must be built with gnome"
	if ! built_with_use dev-scheme/guile regex deprecated discouraged; then
		die "dev-scheme/guile must be built with USE=\"regex deprecated	discouraged\""
	fi
}

src_compile() {
	local myconf_warnings="--enable-error-on-warning --enable-compile-warnings"

	if use doc ; then
		myconf="${myconf} --enable-latex-docs"
	fi

	econf \
		$(use_enable debug) \
		$(use_enable ofx) \
		$(use_enable doc doxygen) \
		$(use_enable doc html-docs) \
		$(use_enable doc dot) \
		$(use_enable hbci) \
		${myconf} || die "econf failed"

	MAKEOPTS="-j1"
	emake || die "emake failed"

    cd "${WORKDIR}/gnucash-docs-${DOC_VER}"
    econf || die "doc econf failed"
    emake || die "doc emake failed"
}

# See http://bugs.gentoo.org/show_bug.cgi?id=132862 regarding gconf schema install

src_install() {
	gnome2_src_install || die "gnome2_src_install failed"
	dodoc AUTHORS ChangeLog* COPYING DOCUMENTERS HACKING INSTALL LICENSE NEWS TODO README* doc/README*
	make_desktop_entry ${P} "GnuCash ${PV}" gnucash-icon.png "GNOME;Office;Finance"

	cd "${WORKDIR}/${PN}-docs-${DOC_VER}"
	make DESTDIR="${D}" \
		scrollkeeper_localstate_dir="${D}/var/lib/scrollkeeper" \
		install || die "doc install failed"
	rm -rf "${D}/var/lib/scrollkeeper"
}