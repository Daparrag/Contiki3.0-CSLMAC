/*
 * This file is part of HiKoB Openlab.
 *
 * HiKoB Openlab is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, version 3.
 *
 * HiKoB Openlab is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with HiKoB Openlab. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2014 HiKoB.
 */

/**
 * \file
 *         uart1.h
 *
 * \author
 *         Damien Hedde <damien.hedde.at.hikob.com>
 *         Olivier Fambon <olivier.fambon.at.inria.fr>
 *
 */

#define BAUD2UBR(baud) baud
typedef int (*uart_input_handler_t)(unsigned char);
void uart1_set_input(uart_input_handler_t input);
inline uart_input_handler_t uart1_get_input_handler();
