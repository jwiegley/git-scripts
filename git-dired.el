;;; git-dired.el --- Hacks to dired for it to play nicer with Git

;; Copyright (C) 2008-2009 John Wiegley <johnw@newartisans.com>

;; Emacs Lisp Archive Entry
;; Filename: git-dired.el
;; Version: 1.0
;; Keywords: git dvcs vc
;; Author: John Wiegley <johnw@newartisans.com>
;; Maintainer: John Wiegley <johnw@newartisans.com>
;; Description: Hacks to dired for it to play nicer with Git
;; URL: http://github.com/jwiegley/git-scripts/tree/master
;; Compatibility: Emacs22, Emacs23

;; This file is not part of GNU Emacs.

;; This is free software; you can redistribute it and/or modify it under
;; the terms of the GNU General Public License as published by the Free
;; Software Foundation; either version 2, or (at your option) any later
;; version.
;;
;; This is distributed in the hope that it will be useful, but WITHOUT
;; ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
;; FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
;; for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; see the file COPYING.  If not, write to the
;; Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
;; MA 02111-1307, USA.

;;; Commentary:

(eval-after-load "dired-x"
  '(progn
     (defvar dired-omit-regexp-orig (symbol-function 'dired-omit-regexp))

     (defun dired-omit-regexp ()
       (let ((file (expand-file-name ".git"))
             parent-dir)
         (while (and (not (file-exists-p file))
                     (progn
                       (setq parent-dir
                             (file-name-directory
                              (directory-file-name
                               (file-name-directory file))))
                       ;; Give up if we are already at the root dir.
                       (not (string= (file-name-directory file)
                                     parent-dir))))
           ;; Move up to the parent dir and try again.
           (setq file (expand-file-name ".git" parent-dir)))
         ;; If we found a change log in a parent, use that.
         (if (file-exists-p file)
             (let ((regexp (funcall dired-omit-regexp-orig))
                   (omitted-files (shell-command-to-string
                                   "git clean -d -x -n")))
               (if (= 0 (length omitted-files))
                   regexp
                 (concat
                  regexp
                  (if (> (length regexp) 0)
                      "\\|" "")
                  "\\("
                  (mapconcat
                   #'(lambda (str)
                       (concat "^"
                               (regexp-quote
                                (substring str 13
                                           (if (= ?/ (aref str (1- (length str))))
                                               (1- (length str))
                                             nil)))
                               "$"))
                   (split-string omitted-files "\n" t)
                   "\\|")
                  "\\)")))
           (funcall dired-omit-regexp-orig))))))

(provide 'git-dired)

;;; git-dired.el ends here
