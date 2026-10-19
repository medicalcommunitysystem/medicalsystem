#ifndef PRESCRIPTION_H
#define PRESCRIPTION_H

#include <stddef.h>
#include "common.h"
#include "medicine.h"

#define PRESC_MED_ID_LEN    21    
#define PRESC_MED_NAME_LEN  51    
#define PRESC_PATIENT_LEN   21    
#define PRESC_DOCTOR_LEN    11    
#define PRESC_DIAG_LEN     201    
#define PRESC_DOSAGE_LEN   101  
#define PRESC_TIME_LEN      24   
#define PRESC_MAX_DETAILS   20    


#define PRESC_STATUS_CANCEL  0
#define PRESC_STATUS_NORMAL  1
#define PRESC_STATUS_RETURN  2 

typedef struct {
    int    presc_id;                    
    int    visit_id;                     
    char   patient_id[PRESC_PATIENT_LEN];
    char   doctor_id[PRESC_DOCTOR_LEN];
    char   diagnosis[PRESC_DIAG_LEN];
    char   create_time[PRESC_TIME_LEN];
    int    status;
    double total_amount;                
} Prescription;

typedef struct {
    long   detail_id;
    char   med_id[PRESC_MED_ID_LEN];
    char   med_name[PRESC_MED_NAME_LEN]; 
    int    quantity;
    char   dosage[PRESC_DOSAGE_LEN];     
    double unit_price;                   
    double subtotal;
} PrescDetail;

double presc_calc_subtotal(int quantity, double unit_price);
double presc_calc_total(const PrescDetail *items, int n);

int presc_add(int visit_id,const char *patient_id, const char *doctor_id,
              const char *diagnosis, PrescDetail *items, int n_items,
              int *out_presc_id);

int presc_cancel(int presc_id);
int presc_return(int presc_id);

int presc_exists(int presc_id);
int presc_get_by_id(int presc_id, Prescription *out);
int presc_get_details(int presc_id, PrescDetail *list, int max);
int presc_query_by_patient(const char *patient_id, Prescription *list, int max);
int presc_query_by_doctor(const char *doctor_id, Prescription *list, int max);
int presc_query_by_date(const char *start_date, const char *end_date,
                        Prescription *list, int max);

#endif 