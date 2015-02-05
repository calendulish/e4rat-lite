/*
 * e4rat-collect.cc - Generate file list of relevant files by monitoring programs
 *
 * Copyright (C) 2015 by Lara Maia <lara@craft.net.br>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INTL_H
#define INTL_H

#include <locale.h>
#include <libintl.h>

#ifndef _
  #ifdef DEBUG_ENABLED
    #define _(string) string
  #else
    #define _(string) gettext(string)
  #endif
#endif

#endif
