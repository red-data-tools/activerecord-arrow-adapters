#!/bin/sh

valgrind --tool=massif \
         --time-unit=B \
         --massif-out-file=massif_arrow.out.%p \
         $(rbenv which ruby) script_arrow_for_massif.rb

valgrind --tool=massif \
         --time-unit=B \
         --massif-out-file=massif_original.out.%p \
         $(rbenv which ruby) script_original_for_massif.rb
