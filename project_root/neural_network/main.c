#include"nn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define EPOCHS       80      // Number of full passes over the training set
#define LR           0.001f  // Base learning rate used by SGD updates
#define TRAIN_SPLIT  0.90f   // Fraction of samples used for training (the rest is test)
#define H  IMAGE_SIZE
#define W  IMAGE_SIZE
#define HO (IMAGE_SIZE/POOL)
#define WO (IMAGE_SIZE/POOL)
static inline int I3(int c,int y,int x,int C,int HH,int WW){ return c*HH*WW + y*WW + x; }

int main(int argc, char **argv){
    srand((unsigned)time(NULL));
    if(argc<2){ usage(argv[0]); return 1; }

    if(strcmp(argv[1],"train")==0){
        if(argc<4){ usage(argv[0]); return 1; }
        Dataset D = load_csv(argv[2]); if(D.n<=0){fprintf(stderr,"empty\n"); return 2;}
        Network net; init_net(&net);

        int ntr = (int)(D.n*TRAIN_SPLIT), nte=D.n-ntr;
        int *perm=malloc(sizeof(int)*ntr); for(int i=0;i<ntr;i++) perm[i]=i;

        for(int e=0;e<EPOCHS;e++){
            shuffle_idx(perm,ntr);
            double L=0.0; for(int t=0;t<ntr;t++){
                int i = perm[t];
                L += train_one(&net, &D.X[i*H*W], D.y[i], LR);
            }
            int ok=0; for(int i=ntr;i<D.n;i++){
                int p = predict(&net, &D.X[i*H*W]); if(p==D.y[i]) ok++;
            }
            printf("epoch %2d  acc=%.2f%%  loss=%.4f\n", e+1, 100.0*ok/nte, (float)(L/ntr));
        }
        if(save_model(argv[3],&net)!=0) fprintf(stderr,"warn: save failed\n");
        free(perm); free_dataset(&D); return 0;
    }
    else if(strcmp(argv[1],"test")==0){
        if(argc<4){ usage(argv[0]); return 1; }
        Dataset D = load_csv(argv[2]); if(D.n<=0){fprintf(stderr,"empty\n"); return 2;}
        Network net; init_net(&net);
        if(load_model(argv[3],&net)!=0){fprintf(stderr,"load fail\n"); return 3;}
        int ok=0; for(int i=0;i<D.n;i++){ if(predict(&net,&D.X[i*H*W])==D.y[i]) ok++; }
        printf("test acc=%.2f%% (%d/%d)\n", 100.0*ok/D.n, ok, D.n);
        free_dataset(&D); return 0;
    }
    else { usage(argv[0]); return 1; }
}