;; test-load-deps.scm : load gnc-mod-baz, which depends on gnc-mod-foo

(use-modules (gnucash gnc-module))

(gnc:module-system-init)
(gnc:module-load "gnucash/baz" 0)

(baz:hello)
(foo:hello)
(baz:scheme-hello)
(foo:scheme-hello)

(exit 0)
