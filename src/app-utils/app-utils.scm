
(define-module (gnucash app-utils))
(use-modules (g-wrapped gw-app-utils))
(use-modules (srfi srfi-1))
(use-modules (gnucash main) (g-wrapped gw-gnc)) ;; FIXME: delete after we finish modularizing.
(use-modules (gnucash gnc-module))

(gnc:module-load "gnucash/engine" 0)

;; c-interface.scm
(export gnc:error->string)
(export gnc:gettext)
(export gnc:_)
(export _)
(export-syntax N_)
(export gnc:make-string-database)

(export gnc:make-option)
(export gnc:option-section)
(export gnc:option-name)
(export gnc:option-sort-tag)
(export gnc:option-type)
(export gnc:option-documentation)
(export gnc:option-getter)
(export gnc:option-setter)
(export gnc:option-default-getter)
(export gnc:option-generate-restore-form)
(export gnc:option-value-validator)
(export gnc:option-data)
(export gnc:option-data-fns)
(export gnc:option-set-changed-callback)
(export gnc:option-strings-getter)
(export gnc:option-widget-changed-proc)
(export gnc:option-value)
(export gnc:option-set-value)
(export gnc:option-index-get-name)
(export gnc:option-index-get-description)
(export gnc:option-index-get-value)
(export gnc:option-value-get-index)
(export gnc:option-number-of-indices)
(export gnc:option-default-value)
(export gnc:restore-form-generator)
(export gnc:value->string)
(export gnc:make-string-option)
(export gnc:make-text-option)
(export gnc:make-font-option)
(export gnc:make-currency-option)
(export gnc:make-commodity-option)
(export gnc:make-simple-boolean-option)
(export gnc:make-complex-boolean-option)
(export gnc:make-pixmap-option)
(export gnc:make-date-option)
(export gnc:get-rd-option-data-subtype)
(export gnc:get-rd-option-data-show-time)
(export gnc:get-rd-option-data-rd-list)
(export gnc:date-option-get-subtype)
(export gnc:date-option-show-time?)
(export gnc:date-option-value-type)
(export gnc:date-option-absolute-time)
(export gnc:date-option-relative-time)
(export gnc:make-account-list-option)
(export gnc:make-account-list-limited-option)
(export gnc:multichoice-list-lookup)
(export gnc:make-multichoice-option)
(export gnc:make-multichoice-callback-option)
(export gnc:make-radiobutton-option)
(export gnc:make-radiobutton-callback-option)
(export gnc:make-list-option)

(export gnc:make-number-range-option)
(export gnc:make-internal-option)
(export gnc:make-query-option)
(export gnc:make-color-option)

(export gnc:color->html)
(export gnc:color-option->html)
(export gnc:color-option->hex-string)
(export gnc:new-options)

(export gnc:register-option)
(export gnc:options-register-callback)
(export gnc:options-register-c-callback)
(export gnc:options-unregister-callback-id)
(export gnc:options-for-each)
(export gnc:options-for-each-general)
(export gnc:lookup-option)
(export gnc:generate-restore-forms)
(export gnc:options-clear-changes)
(export gnc:options-touch)
(export gnc:options-run-callbacks)
(export gnc:options-set-default-section)
(export gnc:options-get-default-section)
(export gnc:options-copy-values)
(export gnc:send-options)
(export gnc:save-options)

;; config-var.scm
(export gnc:make-config-var)
(export gnc:config-var-description-get)
(export gnc:config-var-action-func-get)
(export gnc:config-var-equality-func-get)
(export gnc:config-var-modified?)
(export gnc:config-var-modified?-set!)
(export gnc:config-var-default-value-get)
(export gnc:config-var-default-value-set!)
(export gnc:config-var-value-get)
(export gnc:config-var-value-set!)
(export gnc:config-var-value-is-default?)

;; prefs.scm
(export gnc:register-configuration-option)
(export gnc:lookup-global-option)
(export gnc:send-global-options)
(export gnc:global-options-clear-changes)
(export gnc:save-all-options)
(export gnc:get-debit-string)
(export gnc:get-credit-string)
(export gnc:*options-entries*)
(export gnc:config-file-format-version)
(export gnc:*save-options-hook*)

;; date-utilities.scm

(export gnc:reldate-list)
(export gnc:timepair->secs)
(export gnc:secs->timepair)
(export gnc:timepair->date)
(export gnc:date->timepair)
(export gnc:date-get-year)
(export gnc:date-get-month-day)
(export gnc:date-get-month)
(export gnc:date-get-week-day)
(export gnc:date-get-year-day)
(export gnc:timepair-get-year)
(export gnc:timepair-get-month-day)
(export gnc:timepair-get-month)
(export gnc:timepair-get-week-day)
(export gnc:timepair-get-year-day)
(export gnc:date-get-month-string)
(export gnc:leap-year?)
(export gnc:days-in-year)
(export gnc:days-in-month)
(export gnc:date-to-year-fraction)
(export gnc:date-year-delta)
(export gnc:date-to-month-fraction)
(export gnc:date-to-week-fraction)
(export gnc:date-to-day-fraction)
(export moddatek)
(export decdate)
(export incdate)
(export gnc:timepair-later)
(export gnc:timepair-lt)
(export gnc:timepair-earlier)
(export gnc:timepair-gt)
(export gnc:timepair-le)
(export gnc:timepair-ge)
(export gnc:timepair-eq)
(export gnc:timepair-earlier-date)
(export gnc:timepair-later-date)
(export gnc:timepair-le-date)
(export gnc:timepair-ge-date)
(export gnc:timepair-eq-date)
(export gnc:make-date-interval-list)
(export gnc:make-date-list)
(export make-zdate)
(export SecDelta )
(export DayDelta)
(export WeekDelta )
(export TwoWeekDelta)
(export MonthDelta)
(export QuarterDelta)
(export HalfYearDelta)
(export YearDelta )
(export ThirtyDayDelta)
(export NinetyDayDelta)
(export gnc:deltasym-to-delta)
(export gnc:timepair-delta)
(export gnc:time-elapsed)
(export gnc:timepair-start-day-time)
(export gnc:timepair-end-day-time)
(export gnc:timepair-previous-day)
(export gnc:reldate-get-symbol)
(export gnc:reldate-get-string)
(export gnc:reldate-get-desc)
(export gnc:reldate-get-fn)
(export gnc:make-reldate-hash)
(export gnc:reldate-string-db)
(export gnc:relative-date-values)
(export gnc:relative-date-hash)
(export gnc:get-absolute-from-relative-date)
(export gnc:get-relative-date-strings)
(export gnc:get-relative-date-string)
(export gnc:get-relative-date-desc)
(export gnc:get-start-cal-year)
(export gnc:get-end-cal-year)
(export gnc:get-start-prev-year)
(export gnc:get-end-prev-year)
(export gnc:get-start-cur-fin-year)
(export gnc:get-start-prev-fin-year)
(export gnc:get-end-prev-fin-year)
(export gnc:get-end-cur-fin-year)
(export gnc:get-start-this-month)
(export gnc:get-end-this-month)
(export gnc:get-start-prev-month)
(export gnc:get-end-prev-month)
(export gnc:get-start-current-quarter)
(export gnc:get-end-current-quarter)
(export gnc:get-start-prev-quarter)
(export gnc:get-end-prev-quarter)
(export gnc:get-today)
(export gnc:get-one-month-ago)
(export gnc:get-three-months-ago)
(export gnc:get-six-months-ago)
(export gnc:get-one-year-ago)
(export gnc:reldate-initialize)

;; hooks 
(export gnc:hook-define)
(export gnc:hook-danglers-get)
(export gnc:hook-danglers-set!)
(export gnc:hook-danglers->list)
(export gnc:hook-replace-danglers)
(export gnc:hook-run-danglers)
(export gnc:hook-lookup)
(export gnc:hook-add-dangler)
(export gnc:hook-remove-dangler)
(export gnc:hook-description-get)
(export gnc:hook-name-get)
(export gnc:*startup-hook*)
(export gnc:*shutdown-hook*)
(export gnc:*ui-startup-hook*)
(export gnc:*ui-shutdown-hook*)
(export gnc:*book-opened-hook*)
(export gnc:*new-book-hook*)
(export gnc:*book-closed-hook*)
(export gnc:*report-hook*)

;; simple-obj
(export make-simple-class)
(export simple-obj-getter)
(export simple-obj-setter)
(export simple-obj-print)
(export simple-obj-to-list)
(export simple-obj-from-list)
(export make-simple-obj)

(load-from-path "c-interface.scm")
(load-from-path "config-var.scm")
(load-from-path "options.scm")
(load-from-path "hooks.scm")
(load-from-path "prefs.scm")
(load-from-path "date-utilities.scm")
(load-from-path "simple-obj.scm")

(gnc:hook-add-dangler gnc:*startup-hook*
                      (lambda ()
                        (begin
                          ;; Initialize the C side options code.
                          ;; Must come after the scheme options are loaded.
                          (gnc:c-options-init)

                          ;; Initialize the expression parser.
                          ;; Must come after the C side options initialization.
                          (gnc:exp-parser-init))))

;; add a hook to shut down the expression parser
(gnc:hook-add-dangler gnc:*shutdown-hook*
                      (lambda ()
                        (begin
                          ;; Shutdown the expression parser
                          (gnc:exp-parser-shutdown)

                          ;; This saves global options plus (for the
                          ;; moment) saved report and account tree
                          ;; window parameters. Reports and parameters
                          ;; should probably be in a separate file,
                          ;; with the main data file, or something
                          ;; else.
                          (gnc:save-all-options)

                          ;; Shut down the C side options code
                          (gnc:c-options-shutdown))))
