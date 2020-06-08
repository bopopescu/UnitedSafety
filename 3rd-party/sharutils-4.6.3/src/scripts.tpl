[= autogen5 template x =]
[= (dne " * " "/* ")
=]
 *
[= (gpl "shar" " * ")
;; /* =]
 *
 *  This file defines the shell script strings in C format.
 *  The compiler will reconstruct the original string found in scripts.def.
 *  shar.c will emit these strings into the constructed shar archive.
 *  See "scripts.def" for rationale.
 */
[=

 (define body-text "")
 (make-header-guard "shar") =][=

FOR text    =][=

 (set! body-text (string-append (get "body") "\n"))

 (string-append
     (sprintf "\n\nstatic const char %s_z[%d] = \n"
	(get "name") (+ 1 (string-length body-text))  )

     (def-file-line "body" c-file-line-fmt)
     (kr-string body-text) ";" )        =][=

ENDFOR text

=]

#endif /* [= (. header-guard) =] */
/* end of [= (out-name) =] */
