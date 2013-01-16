<?php

/**
 * Implements hook_drush_command().
 */
function islandora_book_batch_drush_command() {
  $items = array();

  $items['islandora_book_batch_preprocess'] = array(
    'aliases' => array('ibbp'),
    'description' => 'Preprocessed books into database entries.',
    'drupal dependencies' => array('islandora_batch', 'islandora_book', 'islandora_book_batch'),
    'options' => array(
      'type' => array(
        'description' => 'Either "directory" or "zip".',
        'required' => TRUE,
      ),
      'target' => array(
        'description' => 'The target to directory or zip file to scan.',
        'required' => TRUE,
      ),
      'content_models' => array(
        'description' => 'A comma-separated list of content models to assign ' .
          'to the objects. Only applies to the "book" level object.',
        'value' => 'optional',
      ),
      'parent' => array(
        'description' => 'The collection to which the generated items should ' .
          'be added. Defaults to the root Islandora repository PID. Only ' .
          'applies to the "book" level object.',
        'value' => 'optional'
      ),
      'parent_relationship_uri' => array(
        'description' => 'The namespace URI of the relationship to the parent.' .
          ' Defaults to "info:fedora/fedora-system:def/relations-external#".',
        'value' => 'optional',
      ),
      'parent_relationship_pred' => array(
        'description' => 'The predicate of the relationship to the parent. ' .
          'Defaults to "isMemberOfCollection".',
        'value' => 'optional',
      ),
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );

  return $items;
}

function drush_islandora_book_batch_preprocess() {
    // Build one of our preprocessors.
  $connection = islandora_get_tuque_connection();
  $parameters = array(
    'type' => drush_get_option('type'),
    'target' => drush_get_option('target'),
    'parent' => drush_get_option('parent', variable_get('islandora_repository_pid', 'islandora:root')),
    'parent_relationship_uri' => drush_get_option('parent_relationship_uri', 'info:fedora/fedora-system:def/relations-external#'),
    'parent_relationship_pred' => drush_get_option('parent_relationship_pred', 'isMemberOfCollection'),
  );

  if ($content_models = drush_get_option('content_models', FALSE)) {
    $parameters['content_models'] = explode(',', $content_models);
  }
  else {
    $parameters['content_models'] = array('islandora:bookCModel');
  }

  $preprocessor = new IslandoraBookBatch($connection, $parameters);

  // Pass the preprocessor off to run.
  $preprocessed = islandora_batch_handle_preprocessor($preprocessor);
}
