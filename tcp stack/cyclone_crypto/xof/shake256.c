/**
 * @file shake256.c
 * @brief SHAKE256 extendable-output function (XOF)
 *
 * @section License
 *
 * Copyright (C) 2010-2018 Oryx Embedded SARL. All rights reserved.
 *
 * This file is part of CycloneCrypto Eval.
 *
 * This software is provided in source form for a short-term evaluation only. The
 * evaluation license expires 90 days after the date you first download the software.
 *
 * If you plan to use this software in a commercial product, you are required to
 * purchase a commercial license from Oryx Embedded SARL.
 *
 * After the 90-day evaluation period, you agree to either purchase a commercial
 * license or delete all copies of this software. If you wish to extend the
 * evaluation period, you must contact sales@oryx-embedded.com.
 *
 * This evaluation software is provided "as is" without warranty of any kind.
 * Technical support is available as an option during the evaluation period.
 *
 * @section Description
 *
 * SHAKE256 is a function on binary data in which the output can be extended
 * to any desired length. SHAKE256 supports 256 bits of security strength.
 * Refer to FIPS 202 for more details
 *
 * @author Oryx Embedded SARL (www.oryx-embedded.com)
 * @version 1.8.2
 **/

//Switch to the appropriate trace level
#define TRACE_LEVEL CRYPTO_TRACE_LEVEL

//Dependencies
#include "core/crypto.h"
#include "xof/shake256.h"

//Check crypto library configuration
#if (SHAKE256_SUPPORT == ENABLED)

//SHAKE256 object identifier (2.16.840.1.101.3.4.2.12)
const uint8_t shake256Oid[9] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x0C};


/**
 * @brief Digest a message using SHAKE256
 * @param[in] input Pointer to the input data
 * @param[in] inputLen Length of the input data
 * @param[out] output Pointer to the output data
 * @param[in] outputLen Expected length of the output data
 * @return Error code
 **/

error_t shake256Compute(const void *input, size_t inputLen,
   uint8_t *output, size_t outputLen)
{
   Shake256Context *context;

   //Allocate a memory buffer to hold the SHAKE256 context
   context = cryptoAllocMem(sizeof(Shake256Context));
   //Failed to allocate memory?
   if(context == NULL)
      return ERROR_OUT_OF_MEMORY;

   //Initialize the SHAKE256 context
   shake256Init(context);
   //Absorb input data
   shake256Absorb(context, input, inputLen);
   //Finish absorbing phase
   shake256Final(context);
   //Extract data from the squeezing phase
   shake256Squeeze(context, output, outputLen);

   //Free previously allocated memory
   cryptoFreeMem(context);
   //Successful processing
   return NO_ERROR;
}


/**
 * @brief Initialize SHAKE256 context
 * @param[in] context Pointer to the SHAKE256 context to initialize
 **/

void shake256Init(Shake256Context *context)
{
   //SHAKE256 supports 256 bits of security strength
   keccakInit(context, 2 * 256);
}


/**
 * @brief Absorb data
 * @param[in] context Pointer to the SHAKE256 context
 * @param[in] input Pointer to the buffer being hashed
 * @param[in] length Length of the buffer
 **/

void shake256Absorb(Shake256Context *context, const void *input, size_t length)
{
   //Absorb the input data
   keccakAbsorb(context, input, length);
}


/**
 * @brief Finish absorbing phase
 * @param[in] context Pointer to the SHAKE256 context
 **/

void shake256Final(Shake256Context *context)
{
   //Finish absorbing phase (padding byte is 0x1F for XOFs)
   keccakFinal(context, KECCAK_XOF_PAD);
}


/**
 * @brief Extract data from the squeezing phase
 * @param[in] context Pointer to the SHAKE256 context
 * @param[out] output Output string
 * @param[in] length Desired output length, in bytes
 **/

void shake256Squeeze(Shake256Context *context, uint8_t *output, size_t length)
{
   //Extract data from the squeezing phase
   keccakSqueeze(context, output, length);
}

#endif
