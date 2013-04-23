/*=========================================================================
  
  Program:   Insight Segmentation & Registration Toolkit
  Module:    $RCSfile: ImageSetStatistics.cxx,v $
  Language:  C++      
  Date:      $Date: 2009/05/15 13:23:53 $
  Version:   $Revision: 1.2 $

  Copriyght (c) 2002 Insight Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.
  
=========================================================================*/


#include <vector>
#include <cstdlib> 
#include <ctime> 
#include <iostream>
#include "ReadWriteImage.h"
//#include "DoSomethingToImage.cxx"

#include "itkMersenneTwisterRandomVariateGenerator.h"            
#include "itkHistogramMatchingImageFilter.h"
#include "itkMinimumMaximumImageFilter.h"
#include "itkConnectedComponentImageFilter.h"
#include "itkRelabelComponentImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"
#include "itkLabelStatisticsImageFilter.h"
#include "itkNeighborhoodIterator.h"

//  RecursiveAverageImages img1  img2  weight

// We divide the 2nd input image by its mean and add it to the first 
// input image with weight 1/n.  
//The output overwrites the 1st img with the sum.  

//Note: could easily add variance computation
//http://people.revoledu.com/kardi/tutorial/RecursiveStatistic/Time-Variance.htm
#include "itkDiscreteGaussianImageFilter.h"


template <class TImageType> 
void ReadImage(itk::SmartPointer<TImageType> &target, const char *file, bool copy)
{
  //  std::cout << " reading b " << std::string(file) << std::endl;
  typedef itk::ImageFileReader<TImageType> readertype;
  typename readertype::Pointer reader = readertype::New();
  reader->SetFileName(file); 
  reader->Update();   
  if (!copy) target=(reader->GetOutput() ); 
   else
    {
      typedef itk::ImageRegionIteratorWithIndex<TImageType> Iterator;
      Iterator vfIter2( target,  target->GetLargestPossibleRegion() );  
      for(  vfIter2.GoToBegin(); !vfIter2.IsAtEnd(); ++vfIter2 )
	{
	  vfIter2.Set( reader->GetOutput()->GetPixel(vfIter2.GetIndex() ));
	}
    }

}


double TProb(double t, int df)
{
  if (t == 0) return 0;
        double a = 0.36338023;
        double w = atan(t / sqrt((double)df));
        double s = sin(w);
        double c = cos(w);
        
        double t1, t2;
        int j1, j2, k2;

        if (df % 2 == 0)       // even
        {
                t1 = s;
                if (df == 2)   // special case df=2 
                        return (0.5 * (1 + t1));
                t2 = s;
                j1 = -1;
                j2 = 0;
                k2 = (df - 2) / 2;
        }
        else
        {
                t1 = w;
                if (df == 1)            // special case df=1
                        return 1 - (0.5 * (1 + (t1 * (1 - a))));
                t2 = s * c;
                t1 = t1 + t2;
                if (df == 3)            // special case df=3
                        return 1 - (0.5 * (1 + (t1 * (1 - a))));
                j1 = 0;
                j2 = 1;
                k2 = (df - 3)/2;
        }
        for (int i=1; i >= k2; i++)
        {
                j1 = j1 + 2;
                j2 = j2 + 2;
                t2 = t2 * c * c * j1/j2;
                t1 = t1 + t2;
        }
        return 1 - (0.5 * (1 + (t1 * (1 - a * (df % 2)))));
}




template <class TImage>
typename TImage::Pointer 
SmoothImage(typename TImage::Pointer image, float sig)
{
  typedef itk::DiscreteGaussianImageFilter<TImage, TImage> dgf;
  typename dgf::Pointer filter = dgf::New();
  filter->SetVariance(sig);
  filter->SetUseImageSpacingOn();
  filter->SetMaximumError(.01f);
  filter->SetInput(image);
  filter->Update();
  return filter->GetOutput();
}


template <class TInputImage>
//typename TInputImage::Pointer 
void
HistogramMatch(  
	       typename TInputImage::Pointer m_InputFixedImage,  typename TInputImage::Pointer m_InputMovingImage)//  typename TInputImage::Pointer m_OutputMovingImage )
{
  std::cout << " MATCHING INTENSITIES " << std::endl;

  typedef itk::HistogramMatchingImageFilter<TInputImage,TInputImage> FilterType;
  typename FilterType::Pointer filter = FilterType::New();
  filter->SetInput( m_InputMovingImage );
  filter->SetReferenceImage( m_InputFixedImage );
  filter->SetNumberOfHistogramLevels( 256 );
  filter->SetNumberOfMatchPoints( 10 );
  filter->ThresholdAtMeanIntensityOn();
  filter->ThresholdAtMeanIntensityOff();
  filter->Update();
  typename TInputImage::Pointer img =  filter->GetOutput(); 
  
  typedef itk::ImageRegionIteratorWithIndex<TInputImage> Iterator;
  Iterator vfIter( img,   img->GetLargestPossibleRegion() );  
  for(  vfIter.GoToBegin(); !vfIter.IsAtEnd(); ++vfIter )
    {
      m_InputMovingImage->SetPixel(vfIter.GetIndex(), vfIter.Get() );
    }


  return;
}


template <class TImage>
void
LocalMean(typename TImage::Pointer image, unsigned int nhood,  typename TImage::Pointer meanimage )
{

  typename TImage::Pointer localmean=MakeNewImage<TImage>(image,0);  

  typedef itk::ImageRegionIteratorWithIndex<TImage> Iterator;
  Iterator outIter(image, image->GetLargestPossibleRegion() );
  typename TImage::SizeType imagesize=image->GetLargestPossibleRegion().GetSize();
  const unsigned int ImageDimension = 3;

  typedef itk::NeighborhoodIterator<TImage>  iteratorType;
  typename iteratorType::RadiusType rad;
  for( unsigned int j = 0; j < ImageDimension; j++ )    rad[j] = nhood;
  for( outIter.GoToBegin(); !outIter.IsAtEnd(); ++outIter )
    {

      itk::NeighborhoodIterator<TImage>  hoodIt( rad,image ,image->GetLargestPossibleRegion());
      typename TImage::IndexType oindex = outIter.GetIndex();
      hoodIt.SetLocation(oindex);
  
      double fixedMean=0;
      //double movingMean=0;
     
      unsigned int indct;
      unsigned int hoodlen=hoodIt.Size();
      
      //unsigned int inct=0;

      bool takesample = true;

      //double sumj=0;
      //double sumi=0;
      if (takesample)
	{

      //double sumj=0;
      double sumi=0;
      unsigned int cter=0;
      for(indct=0; indct<hoodlen; indct++)
	{	  
	  typename TImage::IndexType index=hoodIt.GetIndex(indct);
	  bool inimage=true;
	  for (unsigned int dd=0; dd<ImageDimension; dd++)
	    {
	      if ( index[dd] < 0 || index[dd] > static_cast<typename TImage::IndexType::IndexValueType>(imagesize[dd]-1) ) inimage=false;
	    }
	  
	  if (inimage)
	    { 
	      sumi+=image->GetPixel(index);
	      cter++;
	    }
	}

      if (cter > 0) fixedMean=sumi/(float)cter;
	}
      
      float val = image->GetPixel(oindex) - fixedMean;
      meanimage->SetPixel( oindex, meanimage->GetPixel(oindex) +fixedMean);
      localmean->SetPixel( oindex, val );
    }


  typedef itk::ImageRegionIteratorWithIndex<TImage> Iterator;
  Iterator vfIter( image,   image->GetLargestPossibleRegion() );  
  for(  vfIter.GoToBegin(); !vfIter.IsAtEnd(); ++vfIter )
    {
      vfIter.Set(  localmean->GetPixel( vfIter.GetIndex() ));
      
    }


      
  return;// localmean;

}




template <class TImage>
//std::vector<unsigned int> 
float 
GetClusterStat(typename TImage::Pointer image, float Tthreshold, unsigned int minSize, unsigned int whichstat,
	       std::string outfn, bool TRUTH)
{
  typedef float InternalPixelType;
  typedef TImage InternalImageType;
  typedef TImage OutputImageType;
  typedef itk::BinaryThresholdImageFilter< InternalImageType, InternalImageType > ThresholdFilterType;
  typedef itk::ConnectedComponentImageFilter< InternalImageType, OutputImageType > FilterType;
  typedef itk::RelabelComponentImageFilter< OutputImageType, OutputImageType > RelabelType;
  
  typename ThresholdFilterType::Pointer threshold = ThresholdFilterType::New();
  typename FilterType::Pointer filter = FilterType::New();
  typename RelabelType::Pointer relabel = RelabelType::New();
  
  InternalPixelType threshold_low, threshold_hi;
  threshold_low = Tthreshold;
  threshold_hi = 1.e9;

  threshold->SetInput (image);
  threshold->SetInsideValue(itk::NumericTraits<InternalPixelType>::One);
  threshold->SetOutsideValue(itk::NumericTraits<InternalPixelType>::Zero);
  threshold->SetLowerThreshold(threshold_low);
  threshold->SetUpperThreshold(threshold_hi);
  threshold->Update();
  
  filter->SetInput (threshold->GetOutput());
  // if (argc > 5)
    {
      int fullyConnected = 1;//atoi( argv[5] );
      filter->SetFullyConnected( fullyConnected );
    }
    relabel->SetInput( filter->GetOutput() );
    relabel->SetMinimumObjectSize( minSize );
    //    relabel->SetUseHistograms(true);
    
  try
    {
    relabel->Update();
    }
  catch( itk::ExceptionObject & excep )
    {
    std::cerr << "Relabel: exception caught !" << std::endl;
    std::cerr << excep << std::endl;
    }
  
  typename TImage::Pointer Clusters=MakeNewImage<TImage>(relabel->GetOutput(),0);  
  //typename TImage::Pointer Clusters=relabel->GetOutput();  
  typedef itk::ImageRegionIteratorWithIndex<TImage> Iterator;
  Iterator vfIter( relabel->GetOutput(),  relabel->GetOutput()->GetLargestPossibleRegion() );  
  
  /*
  
  typename itk::LabelStatisticsImageFilter<TImage,TImage>::Pointer labstat=
    itk::LabelStatisticsImageFilter<TImage,TImage>::New();
  labstat->SetInput(image);
  labstat->SetLabelImage(Clusters);
  labstat->SetUseHistograms(true);
  labstat->Update();
  
  typedef itk::ImageRegionIteratorWithIndex<TImage> Iterator;
  Iterator vfIter( Clusters,   Clusters->GetLargestPossibleRegion() );  
  
  float maximum=0;
  // Relabel the Clusters image with the right statistic
  for(  vfIter.GoToBegin(); !vfIter.IsAtEnd(); ++vfIter )
    {
      if (relabel->GetOutput()->GetPixel(vfIter.GetIndex()) > 0 ) 
	{
	  float pix = relabel->GetOutput()->GetPixel(vfIter.GetIndex());
	  float val;
	  if (whichstat == 0) val=pix;
	  else if (whichstat == 1) val = labstat->GetSum(pix);
	  else if (whichstat == 2) val = labstat->GetMean(pix);
	  else if (whichstat == 3) val = labstat->GetMaximum(pix);
	  if (val > maximum) maximum=val;
	  vfIter.Set(val);
	}
    }
  */
  float maximum=relabel->GetNumberOfObjects();
  float maxtstat=0;
  std::vector<unsigned int> histogram((int)maximum+1);
  std::vector<float> clustersum((int)maximum+1);
  for (int i=0; i<=maximum; i++) 
    {
      histogram[i]=0;
      clustersum[i]=0;
    }
  for(  vfIter.GoToBegin(); !vfIter.IsAtEnd(); ++vfIter )
    {
      if (vfIter.Get() > 0 )
	{
	  float vox=image->GetPixel(vfIter.GetIndex());
	  histogram[(unsigned int)vfIter.Get()]=histogram[(unsigned int)vfIter.Get()]+1;
	  clustersum[(unsigned int)vfIter.Get()]+=vox;
	  if (vox > maxtstat) maxtstat=vox;
	}
    }

  for(  vfIter.GoToBegin(); !vfIter.IsAtEnd(); ++vfIter )
    {
      if (vfIter.Get() > 0 ) 
	{
	  if (whichstat == 0) //size
	    {
	      Clusters->SetPixel( vfIter.GetIndex(), histogram[(unsigned int)vfIter.Get()]  );
	    }
	  if (whichstat == 1) //sum
	    {
	      Clusters->SetPixel( vfIter.GetIndex(), clustersum[(unsigned int)vfIter.Get()] );
	    }
	  if (whichstat == 2) //mean 
	    {
	      Clusters->SetPixel( vfIter.GetIndex(), clustersum[(unsigned int)vfIter.Get()]/(float)histogram[(unsigned int)vfIter.Get()]  );
	    }
	  if (whichstat == 3) //max
	    {
	      Clusters->SetPixel( vfIter.GetIndex(), histogram[(unsigned int)vfIter.Get()]  );
	    }
	}
      else Clusters->SetPixel(vfIter.GetIndex(),0);
    } 

  //  for (int i=0; i<=maximum; i++) 
  //  std::cout << " label " << i << " ct is: " << histogram[i] << std::endl;

  if (TRUTH)
  {
    typedef itk::ImageFileWriter<InternalImageType> writertype;
    typename writertype::Pointer writer = writertype::New();
    writer->SetFileName(  (outfn+std::string("Clusters.nii")).c_str());
    writer->SetInput( Clusters ); 
    writer->Write();   
  }
  
  if (whichstat == 0)  return histogram[1];
  else if (whichstat == 1) 
    {
      float mx=0;
      for (int i=1; i<=maximum; i++) if (clustersum[i] > mx) mx=clustersum[i];
      return mx;
    }
  else if (whichstat == 2)
    {
      float mx=0;
      for (int i=1; i<=maximum; i++) 
	if (clustersum[i]/(float)histogram[i] > mx) mx=clustersum[i]/(float)histogram[i]*1000.0;
      return mx;
    }
  else if (whichstat == 3) return maxtstat*1000.0;
  else return histogram[1];
  
  
}  


  float median(std::vector<float> vec) {
		typedef  std::vector<float>::size_type vec_sz;
		vec_sz size = vec.size();
		
		if (size == 0) return 0;
		  //			throw domain_error("median of an empty vector");
		
		sort(vec.begin(),vec.end());
		
		vec_sz mid = size/2;
		
		return size % 2 == 0
			? (vec[mid] + vec[mid-1]) / 2
			: vec[mid];
 	}

float npdf(std::vector<float> vec, bool opt,  float www) {
		typedef  std::vector<float>::size_type vec_sz;
		vec_sz size = vec.size();
		
		if (size == 0) return 0;
		  //			throw domain_error("median of an empty vector");
		
		float mean=0,var=0;
		float max=-1.e9,min=1.e9;
		for (unsigned int i=0; i<size; i++)
		  {
		    float val = vec[i];
		    if (val > max) max=val;
		    else if (val < min) min=val;
		    float n = (float) (i+1);
		    float wt1 = 1.0/(float)n; 
		    float wt2 = 1.0-wt1;
		    mean = mean*wt2+val*wt1;
		    if ( i > 0)
		      {
			float wt3 = 1.0/( (float) n - 1.0 );
			var = var*wt2 + ( val - mean )*( val - mean)*wt3;
		      }
		  }
  
		if (var == 0) return mean;
		//	else std::cout << " Mean " << mean << " var " << var << std::endl;

		// eval parzen probability 
		std::vector<float> prob(size);
		float maxprob=0;
//		float maxprobval=0;
		float weightedmean=0;
		float weighttotal=0;
		unsigned int maxprobind=0;
//		float sample=0.0;
		float width;
		if (www > 0 ) width = www;  else width = sqrt(var)/2.0;
		//		std::cout << " using width " << width << std::endl;
//		float N=(float)size;
		for (unsigned int j=0; j<size; j++)
		  {
		float sample=vec[j];
		float total=0.0;
		for (unsigned int i=0; i<size; i++)
		  {
		    float delt=vec[i]-sample;
		    delt*=delt;
		    prob[i]=1.0/(2.0*3.1214*width)*exp(-0.5*delt/(width*width));
		    total+=prob[i];
		    //		    maxprobval+=prob[i]
		  }
		if (total > maxprob){ maxprob=total; maxprobind=j; }

		weightedmean+=sample*total;
		weighttotal+=total;
		//		for (unsigned int i=0; i<size; i++) prob[i]=prob[i]/total;
		  }
		
		weightedmean/=weighttotal;
		// pxa = 1./N * total ( gaussian )
		maxprob=vec[maxprobind];
		if ( opt ) return maxprob; 
		else return weightedmean;//vec[maxprobind];
		
  }


  float trimmean(std::vector<float> vec) {
		typedef  std::vector<float>::size_type vec_sz;
		vec_sz size = vec.size();
		
		if (size == 0) return 0;
		  //			throw domain_error("median of an empty vector");
		
		sort(vec.begin(),vec.end());
		
		vec_sz mid = size/2;
		
		float lo=mid-(0.1*size+1);
		float hi=mid+(0.1*size+1);
		lo=0; 
		hi=size;
		float ct=hi-lo;
		float total=0;
		for (unsigned int i=(unsigned int)lo; i<hi; i++) total+=vec[i];
		return total/(float)ct;
	        

 	}



template <unsigned int ImageDimension>
int ImageSetStatistics(int argc, char *argv[])        
{
  typedef float  PixelType;
  typedef itk::Vector<float,ImageDimension>         VectorType;
  typedef itk::Image<VectorType,ImageDimension>     FieldType;
  typedef itk::Image<PixelType,ImageDimension> ImageType;
  typedef itk::ImageFileReader<ImageType> readertype;
  typedef itk::ImageFileWriter<ImageType> writertype;
  typedef typename ImageType::IndexType IndexType;
  typedef typename ImageType::SizeType SizeType;
  typedef typename ImageType::SpacingType SpacingType;
  typedef itk::AffineTransform<double,ImageDimension>   AffineTransformType;
  typedef itk::LinearInterpolateImageFunction<ImageType,double>  InterpolatorType1;
  typedef itk::NearestNeighborInterpolateImageFunction<ImageType,double>  InterpolatorType2;
  typedef itk::ImageRegionIteratorWithIndex<ImageType> Iterator;

  int argct=2;
  std::string fn1 = std::string(argv[argct]); argct++;
  std::string outfn = std::string(argv[argct]);argct++;
  unsigned int whichstat = atoi(argv[argct]);argct++;
  std::string roifn="";
  if (argc > argct){ roifn=std::string(argv[argct]); argct++; }
  float www = 0;
  if (argc > argct) { www=atof(argv[argct]);argct++;}
  unsigned int mchmax= 1; 
  if (argc > argct) { mchmax=atoi(argv[argct]); argct++;}
  unsigned int localmeanrad= 0;
  if (argc > argct) { localmeanrad=atoi(argv[argct]);argct++;}

  //  std::cout <<" roifn " << roifn << " fn1 " << fn1 << " whichstat " << whichstat << std::endl;

  typename ImageType::Pointer outimage = NULL; 
  typename ImageType::Pointer ROIimg = NULL; 

  if (roifn.length() > 4)
    {
      std::cout <<" reading roi image " << roifn << std::endl;
      typename readertype::Pointer reader2 = readertype::New();
      reader2->SetFileName(roifn.c_str()); 
      reader2->UpdateLargestPossibleRegion();
      try
	{   
	  ROIimg = reader2->GetOutput(); 
	}
      catch(...)
	{
	  ROIimg=NULL;
	  std::cout << " Error reading ROI image " << std::endl;
	  //  return 0;
	} 
    
    }


  // now do the recursive average
  const unsigned int maxChar = 512;
  char lineBuffer[maxChar]; 
  char filenm[maxChar];
  unsigned int filecount1=0;
  {
  std::ifstream inputStreamA( fn1.c_str(), std::ios::in );
  if ( !inputStreamA.is_open() )
    {
      std::cout << "Can't open parameter file: " << fn1 << std::endl;  
      return -1;
    }
  while ( !inputStreamA.eof() )
    {

      inputStreamA.getline( lineBuffer, maxChar, '\n' ); 
      
      if ( sscanf( lineBuffer, "%s ",filenm) != 1 )
	{
	  //	  std::cout << "Done.  read " << lineBuffer << " n " << ct1 << " files " << std::endl;
	  //std::cout << std::endl;
	  continue;
	}
      else
	{
	  filecount1++;
	}
    }
  inputStreamA.close();  
  }
  std::cout << " NFiles1 " << filecount1 << std::endl;

  typename ImageType::Pointer meanimage;
  std::vector<typename ImageType::Pointer>  imagestack;
  imagestack.resize(filecount1);
  //  imagestack.fill(NULL);
  std::vector<std::string> filenames(filecount1);
  typename ImageType::Pointer StatImage;
    unsigned int ct = 0;
  std::ifstream inputStreamA( fn1.c_str(), std::ios::in );
  if ( !inputStreamA.is_open() )
    {
      std::cout << "Can't open parameter file: " << fn1 << std::endl;  
      return -1;
    }
  while ( !inputStreamA.eof() )
    {

      inputStreamA.getline( lineBuffer, maxChar, '\n' ); 
      
      if ( sscanf( lineBuffer, "%s ",filenm) != 1 )
	{
	  //	  std::cout << "Done.  read " << lineBuffer << " n " << ct1 << " files " << std::endl;
	  //std::cout << std::endl;
	  continue;
	}
      else
	{
	  filenames[ct]=std::string(filenm);
	  ReadImage<ImageType>(imagestack[ct] , filenm ,false);
	  if (ct ==0)   meanimage=MakeNewImage<ImageType>(imagestack[ct],0);  
	  if (localmeanrad > 0) LocalMean<ImageType>(imagestack[ct],localmeanrad,meanimage);
	  std::cout << " done reading " << (float) ct / (float ) filecount1 << std::endl;
	  ct++;
	}
    }
  inputStreamA.close(); 
  ReadImage<ImageType>( StatImage , filenames[0].c_str() , false);
  
      for (unsigned int mch=0; mch<mchmax; mch++)
      {
  
  if (mch > 0)
    {

	  for (unsigned int j=0; j<filecount1; j++)
	    {
	      ReadImage<ImageType>(imagestack[j] , filenames[j].c_str()  , true);
	      HistogramMatch<ImageType>( StatImage, imagestack[j] );
	    }
	  if (localmeanrad > 0) 
	    { 
	      meanimage->FillBuffer(0);
	      for (unsigned int j=0; j<filecount1; j++)
		{
		  LocalMean<ImageType>(imagestack[j],localmeanrad,meanimage);
		}
	    }

    }

  Iterator vfIter(StatImage , StatImage->GetLargestPossibleRegion() ); 
  std::vector<float> voxels(filecount1); 
  unsigned long nvox=1;
  for (unsigned int i=0; i<ImageDimension; i++) nvox*=StatImage->GetLargestPossibleRegion().GetSize()[i];
     
  ct=0;
      unsigned long  prog = nvox/15;
      for(  vfIter.GoToBegin(); !vfIter.IsAtEnd(); ++vfIter )
	{
	  if (ct % prog == 0) std::cout << " % " << (float) ct / (float) nvox << std::endl;
	  ct++;
	  IndexType ind=vfIter.GetIndex();

	  if (mch == 0) meanimage->SetPixel(ind, meanimage->GetPixel(ind)/filecount1 );
	  for (unsigned int j=0; j<filecount1; j++)
	    {
	      voxels[j]=imagestack[j]->GetPixel(ind);
	    }
	  float stat = 0;
	  switch( whichstat ) 
	    {
	       case 1:
		 stat=npdf(voxels,true,www);
		  if (ct == 1)	std::cout << "the max prob appearance \n";
			break;	
	       case 2:
		 stat=npdf(voxels,false,www);
		 if (ct == 1)	std::cout << "the probabilistically weighted appearance " << www << " \n";
		  
			break;

	       case 3:
		  stat=trimmean(voxels);	
		  if (ct == 1)	std::cout << "the trimmed mean appearance \n";
			break;

	       default:
		  stat=median(voxels);
		  if (ct == 1)	std::cout << "the median appearance \n";
			break;
	    }
	  float sval=stat;
	  if ( localmeanrad > 0 ) sval+= meanimage->GetPixel(ind);
	  StatImage->SetPixel(ind, sval);
	}
      WriteImage<ImageType>(StatImage, outfn.c_str() );
       if ( localmeanrad > 0 )  WriteImage<ImageType>(meanimage, "localmean.nii" );

      }
      std::cout << " Done " << std::endl;
      return 0;
 
  

}
      


int main( int argc, char * argv[] )
{

   
  if ( argc < 4 )     
  { 
    std::cout << "Useage ex:  "<< std::endl; 
    std::cout << argv[0] << " ImageDimension controlslist.txt outimage.nii whichstat {roi.nii}  {parzen var} {matchiters} {localmeanrad}" << std::endl; 
    std::cout << " whichstat = 0:  median,  1:  npdf1  , 2: npdf2 ,  3: trim,  else median " << std::endl;
    return 1;
  }           


   // Get the image dimension
 
  switch ( atoi(argv[1]))
   {
   case 2:
     ImageSetStatistics<2>(argc,argv);
      break;
   case 3:
     ImageSetStatistics<3>(argc,argv);
      break;
   default:
      std::cerr << "Unsupported dimension" << std::endl;
      exit( EXIT_FAILURE );
   }
	
  return 0;

}
