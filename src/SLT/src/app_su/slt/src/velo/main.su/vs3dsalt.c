/* velocity interpolation from VS3D card input */

#include "ghdr.h"
#include "gridhd.h"
#include "comva.h"
#include "par.h"


char *sdoc = 
"VS3DSALT - edit VS3D cards with top of salt \n"
"\n"
"vgrid3d [parameters] <vs3d-cards >3d-vgrid 				\n" 
"\n"
"Required parameters:						 	\n"
"vs3d-cards=       Name of dataset containing VS3D cards 		\n"
"fx=               First x coordinate to output velocity grid 	\n"
"                  (x is the lateral position in the inline direction)	\n"
"fy=               First y coordinate to output velocity grid 	\n"
"                  (y is the lateral position in the crossline direction)\n"
"dx=               x increment to output velocity grid 		\n"
"dy=               y increment to output velocity grid 		\n"
"nx=               number of x positions to output velocity grid 	\n"
"ny=               number of y positions to output velocity grid 	\n"
"nvs=              number of velocity (time/depth) slices to output 	\n"
"dvs=              velocity time/depth slice interval 			\n" 
"                  (in ms or m or ft) to output				\n"
"3d-vgrid=         Name of 3d velocity grid file 			\n"
"\n"
"Optional parameters:							\n"
"nvfmax=4096       maximum number of velocity functions in input vs3d   \n" 
"                  dataset     						\n" 
"ntvmax=256        maximum number of t-v pairs per velocity functions 	\n"
"                  in input vs3d   \n" 
"                  dataset     						\n" 
"votype=1          output velocity type (1=interval 0=rms 2=avergage)	\n"
"ivs=0             velocity slice mode (0=time; 1=depth)	\n"
"tslice=1          =1 time/depth slices output (nx by ny by nvs), 	\n"
"                  =0 velocity vectors (nvs by nx by ny)		\n"
"smx=1             Number of x positions to smooth grid before output \n"
"smy=1             Number of y positions to smooth grid before output \n"
"sms=1             Number of time/depth slices to smooth grid before output \n"
"vscale=1.0        Scale to be applied to output velocity		\n"
"vmin=1000.        Minimum acceptable output velocity			\n" 
"vmax=100000.      Maximum acceptable output velocity 			\n" 
"verbose=1         1=print job progress; 0=no print 			\n" 
"vintpr=0          print interval velocity computed at input VS3D 	\n"
"                  locations (0=no 1=yes)				\n"
"intype=1          rms velocity interpolation to output time interval 	\n"
"                  before converting to interval velocity (0=no l=yes)  \n"
"\n"
" Notes:								\n"
" 1. x is the lateral position in the inline direction 			\n"
"    (NOT the West-East direction !!).					\n" 
"    y is the lateral position in the crossline direction 		\n"
"    (NOT the South-North direction !!).				\n"
" 2. VS3D card has the following format					\n"
"1--4----------16------24------32------40------48------56------64------72 \n"
"VS3D          x1      y1      t1      v1      t2      v2      t3      v3 \n" 
"VS3D                          t4      v4      t5      v5      t6      v6 \n" 
"VS3D                          t7      v7      t8      v8                 \n" 
"VS3D          x2      y2      t1      v1      t2      v2      t3      v3 \n" 
"VS3D                          t4      v4                                 \n" 
"\n"
" where (xi,yi) is the velocity analysis location, (ti,vi) are time and \n"
" stacking velocity pairs. 						\n"
" 3. Output velocity grid file contains nvs time/depth slices of 	\n"
" velocities, i.e., velocity cube is stored in disk as v(x,y,t/z).	\n" 
" (NOT v(t/z,x,y)!), unless tslice=0;					\n"
" 4. Velocity low bound (vmin) and upper bound (vmax) are used to clip 	\n"
" the output velocities.						\n" 
"\n"
"AUTHOR:	   Zhiming Li,       ,	6/25/92   		\n"    
;

main(int argc, char **argv)
{
    	FILE *infp=stdin,*outfp;
	FILE *datafp;

	float fx,fy,dx,dy,dvs,tmax,dt,x,y,tmp;
	int nx,ny,nvs,ivs,votype;
	int smx,smy,sms,one;
	int n1,n2,nxy,nxny,np;
	int *nps, *indx;
	int i, j, ix, iy, is, j0, ip;
    	float *xs, *ys, *tpicks, *vpicks, *vv;
    	float *work, *vx, *vtx, *vxy, *vi;
    	float *fsms,*fsmx,*fsmy,*depth,*time,*zs;
	float *vgrid;
	float vmin, vmax, vscale;
	int vintpr, intype, jj;

	int verbose=1;

	int tslice;
	ghed gh;
	float gmin, gmax;
	int ierr;
	int orient=4, gtype=0;

    	/* get parameters */
    	initargs(argc,argv);
    	askdoc(1);

	/* required parameters */
	if (!getparfloat("fx",&fx)) 
		err(" fx missing ");
	if (!getparfloat("fy",&fy)) 
		err(" fy missing ");
	if (!getparfloat("dx",&dx)) 
		err(" dx missing ");
	if (!getparfloat("dy",&dy)) 
		err(" dy missing ");
	if (!getparint("nx",&nx)) 
		err(" nx missing ");
	if (!getparint("ny",&ny)) 
		err(" ny missing ");
	if (!getparint("nvs",&nvs)) 
		err(" nvs missing ");
	if (!getparfloat("dvs",&dvs)) 
		err(" dvs missing ");
	/* optional parameters */
	if (!getparint("ivs",&ivs)) ivs = 0;
	if (!getparint("votype",&votype)) votype = 1;
	if (!getparint("smx",&smx)) smx = 1;
	if (!getparint("smy",&smy)) smy = 1;
	if (!getparint("sms",&sms)) sms = 1;
	if (!getparfloat("vscale",&vscale)) vscale = 1.0; 
	if (!getparfloat("vmin",&vmin)) vmin = 1000.0; 
	if (!getparfloat("vmax",&vmax)) vmax = 100000.0; 
	if (!getparint("tslice",&tslice)) tslice = 1;
	if (!getparint("vintpr",&vintpr)) vintpr = 0;
	if (!getparint("intype",&intype)) intype = 1;
	if (!getparint("verbose",&verbose)) verbose=1;

	if (tslice==1) {
		orient = 4;
	} else if (tslice==0) {
		orient = 1;
	}

	if (votype==0) {
		gtype = 1;
	} else if(votype==1) {
		gtype = 3;
	} else if(votype==2) {
		gtype = 2;
	}
	

    	/* at most 4096 input (x,y) VS3D cards with at most 256 time-vel 
		pairs each */

	if (!getparint("nvfmax",&n2)) n2 = 4096;
	if (!getparint("ntvmax",&n1)) n1 = 256;

    	/* arrays used to store all VELF card's cdp, time and velocity */
    	xs = (float*)emalloc(n2*sizeof(float));
    	ys = (float*)emalloc(n2*sizeof(float));
    	tpicks = (float*)emalloc(n1*n2*sizeof(float));
    	vpicks = (float*)emalloc(n1*n2*sizeof(float));
    	vv = (float*)emalloc(n1*sizeof(float));
    	time = (float*)emalloc(nvs*sizeof(float));
    	nps = (int*)emalloc(n2*sizeof(int));
	
	bzero(nps,n2*sizeof(int));
    	/* read in VS3D cards */
    	nxy = 0;
	vs3dread(infp,xs,ys,tpicks,vpicks,&nxy,nps,n1,n2);

	fprintf(stderr," %d VS3D cards read \n",nxy);

	/*
	for(i=0;i<nxy;i++) {
		for(j=0;j<nps[i];j++)
			fprintf(stderr,"x=%f y=%f t=%f v=%f \n",
				xs[i],ys[i],tpicks[j+i*n1],vpicks[j+i*n1]); 
		fprintf(stderr,"\n");
	}
	*/

   	if (nxy==0) err("No VS3D card input ! Job aborted");
	
	/* find out the maximum time */
	tmax = 0.; 
	for(i=0;i<nxy;i++) {
		if(tpicks[i*n1+nps[i]-1] > tmax) tmax = tpicks[i*n1+nps[i]-1];
	}
	/* compute constant time intervals*/
	dt = tmax/(nvs-1);
	for(i=0;i<nvs;i++) time[i] = i*dt;

	vi = (float*) emalloc(nxy*nvs*sizeof(float));
	work = (float*) emalloc(nvs*sizeof(float));
	indx = (int*) emalloc(nvs*sizeof(int));
	fsmx = (float*) emalloc(smx*sizeof(float));
	fsmy = (float*) emalloc(smy*sizeof(float));
	fsms = (float*) emalloc(sms*sizeof(float));
	if(tslice==0) vgrid = (float*) emalloc(nvs*nx*ny*sizeof(float));

	depth = (float*) emalloc(nvs*sizeof(float));
	zs = (float*) emalloc(nvs*sizeof(float));
	for(j=0;j<nvs;j++) zs[j] = j * dvs;
	
	one = 1;

	for(i=0;i<nxy;i++) {
		if(verbose==1) fprintf(stderr, 	
			" %d t-v pairs read at x=%f y=%f location\n",
			nps[i],xs[i],ys[i]);
		j0 = i*nvs;
		np = nps[i];

		if(ivs==0) {
			if(intype==1) {	
			/* interpolate vs3d to nvs times */
				lin1d_(tpicks+i*n1,vpicks+i*n1,&np,zs,
                       			vi+j0,&nvs,indx);
				tmax = tpicks[i*n1+np-1];
				/* compute interval velocities using
					dix formula */

				work[0] = vi[j0]*vi[j0];
				for(j=1;j<nvs;j++) {
					if(zs[j]<=tmax) {
						work[j]=(vi[j+j0]*vi[j+j0]*
								zs[j] -  	
				   			vi[j-1+j0]*vi[j-1+j0]*
								zs[j-1]) /dvs;
					} else {
						work[j] = work[j-1]; 
					}
				}
				for(j=0;j<nvs;j++) vi[j0+j] = sqrt(work[j]);
			} else {
				vv[0] = vpicks[i*n1];
				for(j=1;j<np;j++) {
					vv[j]=(vpicks[i*n1+j]*vpicks[i*n1+j]*
						tpicks[i*n1+j] -  	
					      vpicks[i*n1+j-1]*vpicks[i*n1+j-1]*
						tpicks[i*n1+j-1]) /  
				   	      (tpicks[i*n1+j]-tpicks[i*n1+j-1]);

					vv[j] = sqrt(vv[j]);
				}
				bisear_(&np,&nvs,tpicks+i*n1,zs,indx);
				for(j=0;j<nvs;j++) {
					jj = indx[j] - 1;
					if(jj<0 || zs[j]<= tpicks[i*n1] ) {
						vi[j0+j] = vv[0];
					} else if(jj>np-2) {
						vi[j0+j] = vv[np-1];
					} else if(abs(zs[j]-tpicks[i*n1+jj])
						<0.001*dvs){
						vi[j0+j] = vv[jj];
					} else {
						vi[j0+j] = vv[jj+1];
					}   
				}
			}
		} else {
		/* output in depth */
			if(intype==1) {
                        /* interpolate vs3d to nvs times */
                                lin1d_(tpicks+i*n1,vpicks+i*n1,&np,time,
                                        vi+j0,&nvs,indx);
                                tmax = tpicks[i*n1+np-1];
                                /* compute interval velocities using
                                        dix formula */

                                work[0] = vi[j0]*vi[j0];
                                for(j=1;j<nvs;j++) {
                                        if(time[j]<=tmax) {
                                                work[j]=(vi[j+j0]*vi[j+j0]*
                                                                time[j] -
                                                        vi[j-1+j0]*vi[j-1+j0]*
                                                                time[j-1]) /dt;
                                        } else {
                                                work[j] = work[j-1];
                                        }
                                }
                                for(j=0;j<nvs;j++) work[j] = sqrt(work[j]);
			} else {
				vv[0] = vpicks[i*n1];
                                for(j=1;j<np;j++) {
                                        vv[j]=(vpicks[i*n1+j]*vpicks[i*n1+j]*
                                                tpicks[i*n1+j] -
                                              vpicks[i*n1+j-1]*vpicks[i*n1+j-1]*
                                                tpicks[i*n1+j-1]) / 
                                              (tpicks[i*n1+j]-tpicks[i*n1+j-1]);
					vv[j] = sqrt(vv[j]);
                                }
                                bisear_(&np,&nvs,tpicks+i*n1,time,indx);
                                for(j=0;j<nvs;j++) {
                                        jj = indx[j] - 1;
                                        if(jj<0 || time[j]<= tpicks[i*n1] ) {
                                                work[j] = vv[0];
                                        } else if(jj>np-2) {
                                                work[j] = vv[np-1];
                                        } else if(abs(time[j]-tpicks[i*n1+jj])
                                                <0.001*dt){
                                                work[j] = vv[jj];
                                        } else {
                                                work[j] = vv[jj+1];
                                        }  
                                }

			}
			depth[0] = time[0]*work[0]*0.5*0.001;
			for(j=1;j<nvs;j++) {
				depth[j] = depth[j-1]+
					(time[j]-time[j-1])*work[j]*0.5*0.001;
			}
			lin1d_(depth,work,&nvs,zs,vi+j0,&nvs,indx);
		}
		if(vintpr==1) {
			for(j=0;j<nvs;j++) {
				fprintf(stderr, "  time/depth=%g   vint=%g \n",
						 zs[j],vi[j+j0]);
			}
		}
		/* convert to rms velocity if needed */
		if(votype==0) {
			tmp = 0.;
			for(j=1;j<nvs;j++) {
				tmp = tmp + vi[j0+j]*vi[j0+j]*dvs;
				vi[j0+j] = sqrt(tmp/zs[j]);
			}
		/* convert to average velocity if needed */
		} else if(votype==2) {
			tmp = 0.;
			for(j=1;j<nvs;j++) {
				tmp = tmp + vi[j0+j]*dvs;
				vi[j0+j]=tmp/zs[j];
			}
		}
		/* check velocity range and apply scale */
		for(j=0;j<nvs;j++) {
			tmp = vi[j0+j]*vscale;
			if(tmp<vmin) tmp = vmin;	
			if(tmp>vmax) tmp = vmax;	
			vi[j0+j] = tmp;
		}
		/* smooth if needed */
		if(sms>1) smth2d_(vi+j0,work,fsms,fsmx,&nvs,&one,&sms,&one);

	}

	free(tpicks);
	free(vpicks);
	free(nps);
	free(zs);
	free(depth);
	free(time);
	free(fsms);
	
	
	/* output time/depth slices */
	free(work);
	free(indx);
	work = (float*) malloc(nxy*sizeof(float));
	vtx = (float*) malloc(nvs*nx*sizeof(float));
	vx = (float*) malloc(nx*sizeof(float));
	indx = (int*) malloc(nxy*sizeof(int));

	if(smx==1 && smy==1) {
		datafp = stdout;
	} else {
		datafp = etempfile(NULL);
	}

	np = 3;

	fprintf(stderr," Start x-y plane interpolation \n"); 

	/* 2d interpolation */
	for(iy=0;iy<ny;iy++) {
		y = fy + iy*dy;
		for(ix=0;ix<nx;ix++) {
			x = fx + ix*dx;
			intp2d_(xs,ys,vi,&nvs,&nxy,&x,&y,
					vtx+ix*nvs,&np,indx,work);
		}
		if(tslice==1) {	
			for(is=0;is<nvs;is++) {
				for(ix=0;ix<nx;ix++) vx[ix]=vtx[is+ix*nvs];
				ip = (is*nx*ny+iy*nx)*sizeof(float);
				efseek(datafp,ip,0);
				efwrite(vx,sizeof(float),nx,datafp);
			}
		} else {
			bcopy(vtx,vgrid+iy*nx*nvs,nx*nvs*sizeof(float));
		} 
		if(smx==1 && smy==1) {
			fminmax(vtx,nvs*nx,&gmin,&gmax);
			if(iy==0) {
				vmin = gmin;
				vmax = gmax;
			} else {
				if(vmin>gmin) vmin = gmin;
				if(vmax<gmax) vmax = gmax;
			}
		}
	}

	fprintf(stderr," x-y plane interpolation done \n"); 

	free(work);
	free(indx);
	free(vtx);
	free(xs);
	free(ys);
	free(vi);
	free(vx);
	
	if(smx>1 || smy>1) {
		if(tslice==1) rewind(datafp);
		outfp = stdout;
		nxny = nx * ny;
		vxy = (float*) emalloc(nxny*sizeof(float));
		work = (float*) emalloc(nxny*sizeof(float));
		for(is=0;is<nvs;is++) {
			if(tslice==1) {
				efread(vxy,sizeof(float),nxny,datafp);
			} else {
				for(iy=0;iy<ny;iy++)
					for(ix=0;ix<nx;ix++)
						vxy[ix+iy*nx]=
						   vgrid[(iy*nx+ix)*nvs+is];
			}
			smth2d_(vxy,work,fsmx,fsmy,&nx,&ny,&smx,&smy);
			if(tslice==1) {
				efwrite(vxy,sizeof(float),nxny,outfp);
			} else {
				for(iy=0;iy<ny;iy++)
					for(ix=0;ix<nx;ix++)
						vgrid[(iy*nx+ix)*nvs+is]=
							vxy[ix+iy*nx];
			}
			fminmax(vxy,ny*nx,&gmin,&gmax);
			if(is==0) {
				vmin = gmin;
				vmax = gmax;
			} else {
				if(vmin>gmin) vmin = gmin;
				if(vmax<gmin) vmax = gmax;
			}
		}
		free(vxy);
		free(work);
	}
	free(fsmx);
	free(fsmy);

	if(tslice==0) {
		if(smx==1 && smy==1) outfp = datafp; 
		efwrite(vgrid,sizeof(float),nx*ny*nvs,outfp);
	} else {
		if(smx==1 && smy==1) outfp = datafp; 
	}

	fflush(outfp);


	bzero((char*)&gh,GHDRBYTES);
	gh.scale = 1.e-6;
	if(tslice==1) {
		putgval(&gh,"n1",(float)nx);
		putgval(&gh,"n2",(float)ny);
		putgval(&gh,"n3",(float)nvs);
		putgval(&gh,"o1",fx);
		putgval(&gh,"o2",fy);
		putgval(&gh,"o3",0.);
		putgval(&gh,"d1",dx);
		putgval(&gh,"d2",dy);
		putgval(&gh,"d3",dvs);
	} else {
		putgval(&gh,"n1",(float)nvs);
		putgval(&gh,"n2",(float)nx);
		putgval(&gh,"n3",(float)ny);
		putgval(&gh,"o1",0.);
		putgval(&gh,"o2",fx);
		putgval(&gh,"o3",fy);
		putgval(&gh,"d1",dvs);
		putgval(&gh,"d2",dx);
		putgval(&gh,"d3",dy);
	}


	putgval(&gh,"dtype",4.);
	putgval(&gh,"gmin",vmin);
	putgval(&gh,"gmax",vmax);
	putgval(&gh,"gtype",gtype);
	putgval(&gh,"orient",orient);


	ierr = fputghdr(outfp,&gh);
	if(ierr!=0) err("error output grid header ");
	return 0;

}
