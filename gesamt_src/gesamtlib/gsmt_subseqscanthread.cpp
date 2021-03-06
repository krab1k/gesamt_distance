//
// =================================================================
//  This code is distributed under the terms and conditions of the
//  CCP4 Program Suite Licence Agreement as 'Part 2' (Annex 2)
//  software. A copy of the CCP4 licence can be obtained by writing
//  to CCP4, Research Complex at Harwell, Rutherford Appleton
//  Laboratory, Didcot OX11 0FA, UK, or from
//  http://www.ccp4.ac.uk/ccp4license.php.
// =================================================================
//
//    14.06.16   <--  Date of Last Modification.
//                   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  ----------------------------------------------------------------
//
//  **** Module  :  GSMT_SubSeqScanThread <implementation>
//       ~~~~~~~~~
//  **** Project :  GESAMT
//       ~~~~~~~~~
//  **** Classes :  gsmt::SubSeqScanThread
//       ~~~~~~~~~
//
//  (C) E. Krissinel 2008-2016
//
// =================================================================
//

#include <string.h>

#include "gsmt_subseqscanthread.h"
#include "gsmt_defs.h"
#include "memio_.h"

// =================================================================

gsmt::SubSeqScanThread::SubSeqScanThread() : ThreadBase()  {
  s1        = NULL;
  seqSheafs = NULL;
  path0     = NULL;
  index     = NULL;
  packNo    = -1;
  minSeqSim = 0.7;
  seqLen    = 0;
  sheafLen  = 0;
}

gsmt::SubSeqScanThread::~SubSeqScanThread()  {
  freeMemory();
}

void gsmt::SubSeqScanThread::freeMemory()  {
  if (seqSheafs)  {
    delete seqSheafs;
    seqSheafs = NULL;
  }
  if (path0)  {
    delete[] path0;
    path0 = NULL;
  }
}

gsmt::PSeqSheafs gsmt::SubSeqScanThread::takeSeqSheafs()  {
PSeqSheafs s = seqSheafs;
  seqSheafs = NULL;
  return s;
}

void gsmt::SubSeqScanThread::initScaner ( PSequence      seq,
                                          int            subSeqLen,
                                          mmdb::realtype minSeqMatch ) {

  freeMemory();

  s1        = seq;
  seqLen    = s1->getSeqLength();
  minSeqSim = minSeqMatch;
  
  sheafLen  = mmdb::IMin(seqLen,subSeqLen);
  
  seqSheafs = new SeqSheafs ( seqLen-sheafLen+1,sheafLen );
  
}

void gsmt::SubSeqScanThread::run()  {
mmdb::io::File   f;
#ifdef __use_memory_pool
mmdb::io::File   fmem;
gsmt::MemIO      memIO;
#endif
PEntry           entry;
PSubEntry        subEntry;
PSequence        s2;
PSeqSheafItem    item;
mmdb::pstr       fpath,fname,fname0,logmsg;
#ifdef __use_memory_pool
mmdb::pstr       memPool;
int              poolSize;
#endif
mmdb::cpstr      qseq,tseq;
char             archFName[200];
char             L[300];
int              minMatchLen,sheafNo,nSubSeqs,subSeqNo,nEquals;
int              n1,n2,n,m,k,pack_no;

  minMatchLen = int(minSeqSim*sheafLen);
  
  s2 = new gsmt::Sequence();
  n_jobs = index->nPackedSubEntries;
 
  logmsg = NULL;
  fname  = NULL;
  fname0 = NULL;
  fpath  = NULL;
  mmdb::CreateCopy ( fname0,"" );
  
  pack_no = packNo;

  while ((pack_no<index->nPacks) && (*keepRunning))  {

    if (index->pack_index[pack_no]>=0)  {
      
      sprintf ( archFName,seq_pack_name_tempate,pack_no );
      mmdb::CreateCopCat ( fpath,path0,mmdb::io::_dir_sep,archFName );
      f.assign ( fpath,false,true,mmdb::io::GZM_NONE );
      if (!f.reset())  {
        if (verbosity>=0)
          printf ( "\n *** cannot open pack file\n"
                   "       %s\n"
                   "       for reading\n\n",fpath );
      } else  {
      
        n1 = index->pack_index[pack_no];
        if (pack_no==index->nPacks-1)
          n2 = index->nEntries;
        else  {
          n2 = index->pack_index[pack_no+1];
          if (n2<0)
            n2 = index->nEntries;
        }
      
        for (n=n1;(n<n2) && (*keepRunning);n++)  {
        
          entry = index->entries[n];
        
          for (m=0;m<entry->nSubEntries;m++)  {
          
            subEntry = entry->subEntries[m];
            
            if (subEntry->selected)  {
          
              mmdb::CreateCopy ( fname,
                mmdb::io::GetFName(entry->fname,mmdb::io::syskey_all) );

              if (verbosity>1)  {
                if (strcmp(fname,fname0))  {
                  sprintf ( L," %03i/%05i. %s:",pack_no,n-n1,fname );
                  mmdb::CreateCopy ( fname0,fname );
                } else  {
                  k = strlen(fname) + 13;
                  for (int i=0;i<k;i++)
                    L[i] = ' ';
                  L[k] = char(0);
                }
                mmdb::CreateCopy ( logmsg,L );
                sprintf ( L,"%s  %5i residues:",
                            subEntry->id,subEntry->size );
                mmdb::CreateConcat ( logmsg,L );
              }

              if (sheafLen<=subEntry->size)  {
          
                f.seek ( subEntry->offset_seq );

#ifdef __use_memory_pool
                memIO.read ( f );
                memIO.get_buffer ( &memPool,&poolSize );
                fmem.assign ( poolSize,0,memPool );
                fmem.reset();
                s2->read ( fmem );
                fmem.shut();
#else
                s2->read ( f );
#endif

                nSubSeqs = subEntry->size - sheafLen + 1;
                for (sheafNo=0;sheafNo<seqSheafs->nSheafs;sheafNo++)
                  for (subSeqNo=0;subSeqNo<nSubSeqs;subSeqNo++)  {
                
                    nEquals = 0;
                    qseq    = &(s1->getSequence()[sheafNo]);
                    tseq    = &(s2->getSequence()[subSeqNo]);
                    for (int i=0;i<sheafLen;i++)
                      if (*qseq++==*tseq++)
                        nEquals++;

                    if (nEquals>=minMatchLen)  {

                      item = new SeqSheafItem();
                      item->score = mmdb::realtype(nEquals)/sheafLen;
                      strcpy ( item->id,entry->id );
//if (!strcmp(entry->id,"1SAR"))
//  printf ( " 1SAR:  score=%.4f\n",item->score );
                      item->startPos = subSeqNo;
                      item->packNo   = pack_no;
                      item->subEntry.copy ( subEntry );
                      seqSheafs->sheafs[sheafNo]->addItem ( item );
                    
                      if (verbosity>1)  {
                        sprintf ( L," Q:T = %03i:%03i score=%5.3f\n",
                                    sheafNo,subSeqNo,item->score );
//                      mmdb::CreateConcat ( logmsg,L );
                        printf ( "%s%s",logmsg,L );
                      }

                    }
                  }
    
              } else if (verbosity>1)
                printf ( "%s => filtered out by size (%i residues) \n",
                         logmsg,subEntry->size );

            }

            job_cnt++;
            if (pbar && (threadNo==0))
              displayProgress();

          }

        }

        f.shut();

      }

    }

    pack_no += nthreads;

  }

  delete s2;

  if (fpath)  delete[] fpath;
  if (fname)  delete[] fname;
  if (fname0) delete[] fname0;
  if (logmsg) delete[] logmsg;

  finished = true;

}

