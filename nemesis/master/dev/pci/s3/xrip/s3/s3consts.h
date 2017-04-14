
extern int s3RamdacType;

#define UNKNOWN_DAC       -1
#define NORMAL_DAC         0
#define BT485_DAC          1
#define ATT20C505_DAC      2
#define TI3020_DAC         3
#define ATT498_DAC         4
#define ATT20C498_DAC      ATT498_DAC
#define TI3025_DAC         5
#define ATT20C490_DAC      6
#define SC15025_DAC        7
#define STG1700_DAC        8
#define S3_SDAC_DAC        9
#define S3_GENDAC_DAC     10
#define ATT22C498_DAC     11
#define S3_TRIO32_DAC     12
#define S3_TRIO64_DAC     13
#define TI3026_DAC        14
#define IBMRGB52x_DAC     15
#define IBMRGB524_DAC     16
#define IBMRGB525_DAC     17
#define IBMRGB528_DAC     18
#define STG1703_DAC       19
#define SC1148x_M2_DAC    20
#define SC1148x_M3_DAC    21

#define DAC_IS_BT485_SERIES	(s3RamdacType == BT485_DAC || \
				 s3RamdacType == ATT20C505_DAC)
#define DAC_IS_TI3020_SERIES	(s3RamdacType == TI3020_DAC || \
				 s3RamdacType == TI3025_DAC)
#define DAC_IS_TI3020		(s3RamdacType == TI3020_DAC)
#define DAC_IS_TI3025		(s3RamdacType == TI3025_DAC)
#define DAC_IS_TI3026		(s3RamdacType == TI3026_DAC)
#define DAC_IS_ATT20C498	(s3RamdacType == ATT20C498_DAC)
#define DAC_IS_ATT22C498	(s3RamdacType == ATT22C498_DAC)
#define DAC_IS_ATT498		(DAC_IS_ATT20C498 || DAC_IS_ATT22C498)
#define DAC_IS_ATT490		(s3RamdacType == ATT20C490_DAC)
#define DAC_IS_SC15025		(s3RamdacType == SC15025_DAC)
#define DAC_IS_STG1703          (s3RamdacType == STG1703_DAC)
#define DAC_IS_STG1700          (s3RamdacType == STG1700_DAC || DAC_IS_STG1703)
#define DAC_IS_SDAC             (s3RamdacType == S3_SDAC_DAC)
#define DAC_IS_GENDAC           (s3RamdacType == S3_GENDAC_DAC)
#define DAC_IS_TRIO32           (s3RamdacType == S3_TRIO32_DAC)
#define DAC_IS_TRIO64           (s3RamdacType == S3_TRIO64_DAC)
#define DAC_IS_TRIO             (DAC_IS_TRIO32 || DAC_IS_TRIO64)
#define DAC_IS_IBMRGB524        (s3RamdacType == IBMRGB524_DAC)
#define DAC_IS_IBMRGB525        (s3RamdacType == IBMRGB525_DAC)
#define DAC_IS_IBMRGB528        (s3RamdacType == IBMRGB528_DAC)
#define DAC_IS_IBMRGB           (DAC_IS_IBMRGB524 || DAC_IS_IBMRGB525 || DAC_IS_IBMRGB528 )
#define DAC_IS_SC1148x_M2	(s3RamdacType == SC1148x_M2_DAC)
#define DAC_IS_SC1148x_M3	(s3RamdacType == SC1148x_M3_DAC)
#define DAC_IS_SC1148x_SERIES	(DAC_IS_SC1148x_M2 || DAC_IS_SC1148x_M3)

/* Vendor BIOS types */

#define UNKNOWN_BIOS		-1
#define ELSA_BIOS		 1
#define MIRO_BIOS		 2
#define SPEA_BIOS		 3
#define GENOA_BIOS		 4
#define STB_BIOS		 5
#define NUMBER_NINE_BIOS	 6
#define HERCULES_BIOS		 7
#define DIAMOND_BIOS		 8


