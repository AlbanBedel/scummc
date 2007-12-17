/* ScummC
 * Copyright (C) 2004-2007  Alban Bedel
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

/**
 * @file scc_box.c
 * @brief Common box stuff
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "scc_box.h"


void scc_box_list_free(scc_box_t* box) {
  scc_box_t* n;
  while(box) {
    n = box->next;
    free(box);
    box = n;
  }
}

int scc_box_add_pts(scc_box_t* box,int x,int y) {
  int i;
  if(box->npts >= 4) return 0;

  // look if don't alredy have this point
  for(i = 0 ; i < box->npts ; i++)
    if(box->pts[i].x == x && box->pts[i].y == y) return 1;

  box->pts[i].x = x;
  box->pts[i].y = y;

  box->npts++;
  return 1;
}

int scc_box_are_neighbors(scc_box_t* box,int n1,int n2) {
  scc_box_t *a = NULL,*b = NULL,*c;
  int n,m;
  int i,j;
  int f,f2,l,l2;

  // box 0 is connected to nothing
  if((!n1) || (!n2)) return 0;

  for(n = 1, c = box ; c ; n++,c = c->next) {
    if(n == n1) a = c;
    if(n == n2) b = c;
  }

  if(!(a && b)) {
    printf("Failed to find boxes %d and/or %d.\n",n1,n2);
    return 0;
  }

  if((a->flags & SCC_BOX_INVISIBLE) || (b->flags & SCC_BOX_INVISIBLE))
    return 0;

  for(n = 0 ; n < a->npts ; n++) {
    m = (n+1)%a->npts;
    // is it an vertical side
    if(a->pts[n].x == a->pts[m].x) {
      // look at tho other box
      for(i = 0 ; i < b->npts ; i++) {
	j = (i+1)%b->npts;
	// ignore side which are not on the same vertical
	if((b->pts[i].x != a->pts[n].x) ||
	   (b->pts[j].x != a->pts[n].x)) continue;

	if(a->pts[n].y < a->pts[m].y)
	  f = n, l = m;
	else
	  f = m, l = n;

	if(b->pts[i].y < b->pts[j].y)
	  f2 = i, l2 = j;
	else
	  f2 = j, l2 = i;

	if((a->pts[l].y < b->pts[f2].y) || 
	   (a->pts[f].y > b->pts[l2].y)) continue;
	return 1;
      }
      
    }
    // is it an horizontal line
    if(a->pts[n].y == a->pts[m].y) {
      // look at the other box
      for(i = 0 ; i < b->npts ; i++) {
	j = (i+1)%b->npts;
	if((b->pts[i].y != a->pts[n].y) ||
	   (b->pts[j].y != a->pts[n].y)) continue;

	if(a->pts[n].x < a->pts[m].x)
	  f = n, l = m;
	else
	  f = m, l = n;

	if(b->pts[i].x < b->pts[j].x)
	  f2 = i, l2 = j;
	else
	  f2 = j, l2 = i;

	if((a->pts[l].x < b->pts[f2].x) || 
	   (a->pts[f].x > b->pts[l2].x)) continue;
	return 1;
      }
    }
  }
  return 0;
}

// code pumped from scummvm
// changed to make it work with any matrix size
int scc_box_get_matrix(scc_box_t* box,uint8_t** ret) {
  scc_box_t* b;
  int i,j,k,num = 0;
  uint8_t *adjacentMatrix, *itineraryMatrix;
  
  // count the boxes
  for(b = box ; b ; b = b->next) num++;
  // add the box 0
  num++;

  // Allocate the adjacent & itinerary matrices
  adjacentMatrix = malloc(num * num);
  itineraryMatrix = malloc(num * num);

  // Initialise the adjacent matrix: each box has distance 0 to itself,
  // and distance 1 to its direct neighbors. Initially, it has distance
  // 255 (= infinity) to all other boxes.
  for (i = 0; i < num; i++) {
    for (j = 0; j < num; j++) {
      if (i == j) {
	adjacentMatrix[i*num+j] = 0;
	itineraryMatrix[i*num+j] = j;
      } else if (scc_box_are_neighbors(box,i,j)) {
	adjacentMatrix[i*num+j] = 1;
	itineraryMatrix[i*num+j] = j;
      } else {
	adjacentMatrix[i*num+j] = 255;
	itineraryMatrix[i*num+j] = 255;
      }
    }
  }

  // Compute the shortest routes between boxes via Kleene's algorithm.
  for (k = 0; k < num; k++) {
    for (i = 0; i < num; i++) {
      for (j = 0; j < num; j++) {
	uint8_t distIK,distKJ;
	if (i == j)
	  continue;
	distIK = adjacentMatrix[num*i+k];
	distKJ = adjacentMatrix[num*k+j];
	if (adjacentMatrix[num*i+j] > distIK + distKJ) {
	  adjacentMatrix[num*i+j] = distIK + distKJ;
	  itineraryMatrix[num*i+j] = itineraryMatrix[num*i+k];
	}
      }
    }  
  }

  free(adjacentMatrix);
  ret[0] = itineraryMatrix;
  return num;
}

